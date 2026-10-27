#include "../include/TelemetryHistoryManager.h"
#include "../include/json.hpp"
#include <iostream>
#include <sstream>

using json = nlohmann::json;

TelemetryHistoryManager::TelemetryHistoryManager(std::shared_ptr<IStorage> storage)
    : storage_(std::move(storage)) {}

bool TelemetryHistoryManager::initSchema() {
    if (!storage_ || !storage_->isOpen()) {
        return false;
    }

    const std::string createTableSql =
        "CREATE TABLE IF NOT EXISTS telemetry_history ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  device_name TEXT NOT NULL,"
        "  device_type TEXT NOT NULL,"
        "  serial_number TEXT,"
        "  timestamp INTEGER NOT NULL,"
        "  is_on INTEGER DEFAULT 0,"
        "  temperature REAL DEFAULT 0.0,"
        "  target_temperature REAL DEFAULT 0.0,"
        "  humidity REAL DEFAULT 0.0,"
        "  power REAL DEFAULT 0.0,"
        "  brightness INTEGER DEFAULT 0,"
        "  valve_level INTEGER DEFAULT 0,"
        "  target_valve_level INTEGER DEFAULT 0,"
        "  current_valve_level INTEGER DEFAULT 0,"
        "  mode TEXT DEFAULT 'OFF',"
        "  payload_json TEXT NOT NULL"
        ");";

    if (!storage_->execute(createTableSql)) {
        return false;
    }

    storage_->execute("CREATE INDEX IF NOT EXISTS idx_telemetry_dev ON telemetry_history(device_name);");
    storage_->execute("CREATE INDEX IF NOT EXISTS idx_telemetry_time ON telemetry_history(timestamp);");
    storage_->execute("CREATE INDEX IF NOT EXISTS idx_telemetry_dev_time ON telemetry_history(device_name, timestamp);");

    return true;
}

bool TelemetryHistoryManager::saveTelemetry(const DeviceTelemetry& telemetry) {
    if (!storage_ || !storage_->isOpen()) {
        return false;
    }

    if (telemetry.deviceName.empty()) {
        return false;
    }

    const std::string insertSql =
        "INSERT INTO telemetry_history ("
        "  device_name, device_type, serial_number, timestamp, is_on,"
        "  temperature, target_temperature, humidity, power, brightness,"
        "  valve_level, target_valve_level, current_valve_level, mode, payload_json"
        ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    int64_t tsMs = timePointToMs(telemetry.timestamp);

    std::vector<std::string> params = {
        telemetry.deviceName,
        telemetry.deviceType,
        telemetry.serialNumber,
        std::to_string(tsMs),
        telemetry.isOn ? "1" : "0",
        std::to_string(telemetry.currentTemperature),
        std::to_string(telemetry.targetTemperature),
        std::to_string(telemetry.currentHumidity),
        std::to_string(telemetry.currentPower),
        std::to_string(telemetry.brightness),
        std::to_string(telemetry.valveLevel),
        std::to_string(telemetry.targetValveLevel),
        std::to_string(telemetry.currentValveLevel),
        telemetry.mode,
        telemetry.rawJson
    };

    return storage_->execute(insertSql, params);
}

std::vector<DeviceTelemetry> TelemetryHistoryManager::getHistory(const std::string& deviceName, int limit) const {
    if (!storage_ || !storage_->isOpen()) {
        return {};
    }

    const std::string sql =
        "SELECT * FROM telemetry_history WHERE device_name = ? ORDER BY timestamp DESC LIMIT ?;";

    std::vector<std::string> params = { deviceName, std::to_string(limit) };
    auto rows = storage_->query(sql, params);

    std::vector<DeviceTelemetry> result;
    result.reserve(rows.size());
    for (const auto& row : rows) {
        result.push_back(rowToTelemetry(row));
    }
    return result;
}

std::vector<DeviceTelemetry> TelemetryHistoryManager::getAllHistory(int limit) const {
    if (!storage_ || !storage_->isOpen()) {
        return {};
    }

    const std::string sql =
        "SELECT * FROM telemetry_history ORDER BY timestamp DESC LIMIT ?;";

    std::vector<std::string> params = { std::to_string(limit) };
    auto rows = storage_->query(sql, params);

    std::vector<DeviceTelemetry> result;
    result.reserve(rows.size());
    for (const auto& row : rows) {
        result.push_back(rowToTelemetry(row));
    }
    return result;
}

std::vector<DeviceTelemetry> TelemetryHistoryManager::getHistoryRange(const std::string& deviceName,
                                                                      int64_t startTimestampMs,
                                                                      int64_t endTimestampMs) const {
    if (!storage_ || !storage_->isOpen()) {
        return {};
    }

    const std::string sql =
        "SELECT * FROM telemetry_history WHERE device_name = ? AND timestamp >= ? AND timestamp <= ? "
        "ORDER BY timestamp ASC;";

    std::vector<std::string> params = {
        deviceName,
        std::to_string(startTimestampMs),
        std::to_string(endTimestampMs)
    };

    auto rows = storage_->query(sql, params);
    std::vector<DeviceTelemetry> result;
    result.reserve(rows.size());
    for (const auto& row : rows) {
        result.push_back(rowToTelemetry(row));
    }
    return result;
}

std::optional<DeviceTelemetry> TelemetryHistoryManager::getLatestRecorded(const std::string& deviceName) const {
    auto history = getHistory(deviceName, 1);
    if (!history.empty()) {
        return history.front();
    }
    return std::nullopt;
}

size_t TelemetryHistoryManager::getHistoryCount(const std::string& deviceName) const {
    if (!storage_ || !storage_->isOpen()) {
        return 0;
    }

    IStorage::QueryResult rows;
    if (deviceName.empty()) {
        rows = storage_->query("SELECT COUNT(*) AS cnt FROM telemetry_history;");
    } else {
        rows = storage_->query("SELECT COUNT(*) AS cnt FROM telemetry_history WHERE device_name = ?;", { deviceName });
    }

    if (!rows.empty()) {
        auto it = rows[0].find("cnt");
        if (it != rows[0].end() && !it->second.empty()) {
            try {
                return static_cast<size_t>(std::stoull(it->second));
            } catch (...) {
                return 0;
            }
        }
    }
    return 0;
}

bool TelemetryHistoryManager::clearHistory(const std::string& deviceName) {
    if (!storage_ || !storage_->isOpen()) {
        return false;
    }

    if (deviceName.empty()) {
        return storage_->execute("DELETE FROM telemetry_history;");
    } else {
        return storage_->execute("DELETE FROM telemetry_history WHERE device_name = ?;", { deviceName });
    }
}

std::shared_ptr<IStorage> TelemetryHistoryManager::getStorage() const {
    return storage_;
}

int64_t TelemetryHistoryManager::timePointToMs(const std::chrono::system_clock::time_point& tp) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count();
}

std::chrono::system_clock::time_point TelemetryHistoryManager::msToTimePoint(int64_t ms) {
    return std::chrono::system_clock::time_point(std::chrono::milliseconds(ms));
}

DeviceTelemetry TelemetryHistoryManager::rowToTelemetry(const IStorage::Row& row) const {
    DeviceTelemetry t;

    auto getVal = [&](const std::string& col) -> std::string {
        auto it = row.find(col);
        return (it != row.end()) ? it->second : "";
    };

    t.deviceName = getVal("device_name");
    t.deviceType = getVal("device_type");
    t.serialNumber = getVal("serial_number");
    t.rawJson = getVal("payload_json");
    t.mode = getVal("mode");

    std::string tsStr = getVal("timestamp");
    if (!tsStr.empty()) {
        try {
            t.timestamp = msToTimePoint(std::stoll(tsStr));
        } catch (...) {
            t.timestamp = std::chrono::system_clock::now();
        }
    }

    std::string onStr = getVal("is_on");
    t.isOn = (onStr == "1" || onStr == "true");

    try {
        std::string s = getVal("temperature");
        if (!s.empty()) t.currentTemperature = std::stof(s);
    } catch (...) {}

    try {
        std::string s = getVal("target_temperature");
        if (!s.empty()) t.targetTemperature = std::stof(s);
    } catch (...) {}

    try {
        std::string s = getVal("humidity");
        if (!s.empty()) t.currentHumidity = std::stof(s);
    } catch (...) {}

    try {
        std::string s = getVal("power");
        if (!s.empty()) t.currentPower = std::stof(s);
    } catch (...) {}

    try {
        std::string s = getVal("brightness");
        if (!s.empty()) t.brightness = std::stoi(s);
    } catch (...) {}

    try {
        std::string s = getVal("valve_level");
        if (!s.empty()) t.valveLevel = std::stoi(s);
    } catch (...) {}

    try {
        std::string s = getVal("target_valve_level");
        if (!s.empty()) t.targetValveLevel = std::stoi(s);
    } catch (...) {}

    try {
        std::string s = getVal("current_valve_level");
        if (!s.empty()) t.currentValveLevel = std::stoi(s);
    } catch (...) {}

    // Populate numeric metrics map
    t.numericMetrics["temperature"] = t.currentTemperature;
    t.numericMetrics["targetTemperature"] = t.targetTemperature;
    t.numericMetrics["humidity"] = t.currentHumidity;
    t.numericMetrics["power"] = t.currentPower;
    t.numericMetrics["brightness"] = t.brightness;
    t.numericMetrics["valveLevel"] = t.valveLevel;
    t.numericMetrics["isOn"] = t.isOn ? 1.0 : 0.0;

    return t;
}
