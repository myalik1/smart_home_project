#include "net/DtoModels.h"

#include <QJsonDocument>
#include <QJsonParseError>

namespace smarthome::net {
namespace {

bool isPresent(const QJsonObject &obj, const QString &key) {
    const QJsonValue val = obj.value(key);
    return !val.isUndefined() && !val.isNull();
}

std::optional<QString> readOptionalString(const QJsonObject &obj, const QString &key) {
    if (!isPresent(obj, key)) {
        return std::nullopt;
    }
    return obj.value(key).toString();
}

std::optional<bool> readOptionalBool(const QJsonObject &obj, const QString &key) {
    if (!isPresent(obj, key)) {
        return std::nullopt;
    }
    return obj.value(key).toBool();
}

std::optional<int> readOptionalInt(const QJsonObject &obj, const QString &key) {
    if (!isPresent(obj, key)) {
        return std::nullopt;
    }
    return obj.value(key).toInt();
}

std::optional<qint64> readOptionalInt64(const QJsonObject &obj, const QString &key) {
    if (!isPresent(obj, key)) {
        return std::nullopt;
    }
    return static_cast<qint64>(obj.value(key).toDouble());
}

std::optional<RuleAction> readOptionalAction(const QJsonObject &obj, const QString &key) {
    const QJsonValue val = obj.value(key);
    if (!val.isObject()) {
        return std::nullopt;
    }
    return RuleAction::fromJson(val.toObject());
}

} // namespace

DeviceTelemetry DeviceTelemetry::fromJson(const QJsonObject &obj) {
    DeviceTelemetry dto;
    dto.deviceName = obj.value(QStringLiteral("deviceName")).toString();
    dto.deviceType = obj.value(QStringLiteral("deviceType")).toString(QStringLiteral("Unknown"));
    if (isPresent(obj, QStringLiteral("serialNumber"))) {
        dto.serialNumber = obj.value(QStringLiteral("serialNumber")).toString();
    }
    if (isPresent(obj, QStringLiteral("timestamp"))) {
        dto.timestamp = static_cast<qint64>(obj.value(QStringLiteral("timestamp")).toDouble());
    }
    if (isPresent(obj, QStringLiteral("timestampIso"))) {
        dto.timestampIso = obj.value(QStringLiteral("timestampIso")).toString();
    }

    dto.isOn = obj.value(QStringLiteral("isOn")).toBool(false);
    dto.currentTemperature = obj.value(QStringLiteral("currentTemperature")).toDouble(0.0);
    dto.targetTemperature = obj.value(QStringLiteral("targetTemperature")).toDouble(0.0);
    dto.currentHumidity = obj.value(QStringLiteral("currentHumidity")).toDouble(0.0);
    dto.currentPower = obj.value(QStringLiteral("currentPower")).toDouble(0.0);
    dto.brightness = obj.value(QStringLiteral("brightness")).toInt(0);
    dto.valveLevel = obj.value(QStringLiteral("valveLevel")).toInt(0);
    dto.targetValveLevel = obj.value(QStringLiteral("targetValveLevel")).toInt(0);
    dto.currentValveLevel = obj.value(QStringLiteral("currentValveLevel")).toInt(0);
    dto.mode = obj.value(QStringLiteral("mode")).toString(QStringLiteral("OFF"));
    return dto;
}

std::optional<DeviceTelemetry> DeviceTelemetry::fromJsonBytes(const QByteArray &bytes, QString *errorOut) {
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (errorOut != nullptr) {
            *errorOut = parseError.errorString();
        }
        return std::nullopt;
    }
    if (!doc.isObject()) {
        if (errorOut != nullptr) {
            *errorOut = QStringLiteral("Expected JSON object for DeviceTelemetry");
        }
        return std::nullopt;
    }

    DeviceTelemetry dto = DeviceTelemetry::fromJson(doc.object());
    if (!dto.isValid()) {
        if (errorOut != nullptr) {
            *errorOut = QStringLiteral("Missing required field 'deviceName' in DeviceTelemetry");
        }
        return std::nullopt;
    }
    return dto;
}

QVector<DeviceTelemetry> DeviceTelemetry::listFromJson(const QJsonArray &arr) {
    QVector<DeviceTelemetry> result;
    result.reserve(arr.size());
    for (const QJsonValue &item : arr) {
        if (item.isObject()) {
            result.append(DeviceTelemetry::fromJson(item.toObject()));
        }
    }
    return result;
}

QJsonObject DeviceTelemetry::toJson() const {
    QJsonObject obj;
    obj.insert(QStringLiteral("deviceName"), deviceName);
    obj.insert(QStringLiteral("deviceType"), deviceType);
    obj.insert(QStringLiteral("serialNumber"), serialNumber.isEmpty() ? QJsonValue::Null : QJsonValue(serialNumber));
    obj.insert(QStringLiteral("timestamp"), static_cast<qint64>(timestamp));
    obj.insert(QStringLiteral("timestampIso"), timestampIso.isEmpty() ? QJsonValue::Null : QJsonValue(timestampIso));
    obj.insert(QStringLiteral("isOn"), isOn);
    obj.insert(QStringLiteral("currentTemperature"), currentTemperature);
    obj.insert(QStringLiteral("targetTemperature"), targetTemperature);
    obj.insert(QStringLiteral("currentHumidity"), currentHumidity);
    obj.insert(QStringLiteral("currentPower"), currentPower);
    obj.insert(QStringLiteral("brightness"), brightness);
    obj.insert(QStringLiteral("valveLevel"), valveLevel);
    obj.insert(QStringLiteral("targetValveLevel"), targetValveLevel);
    obj.insert(QStringLiteral("currentValveLevel"), currentValveLevel);
    obj.insert(QStringLiteral("mode"), mode);
    return obj;
}

DeviceStatus DeviceStatus::fromJson(const QJsonObject &obj) {
    DeviceStatus dto;
    dto.deviceName = obj.value(QStringLiteral("deviceName")).toString();
    dto.deviceType = obj.value(QStringLiteral("deviceType")).toString();
    dto.topic = obj.value(QStringLiteral("topic")).toString();
    dto.isOnline = obj.value(QStringLiteral("isOnline")).toBool(false);
    dto.lastSeenMs = readOptionalInt64(obj, QStringLiteral("lastSeenMs"));

    const QJsonValue telemetryVal = obj.value(QStringLiteral("telemetry"));
    if (telemetryVal.isObject()) {
        dto.telemetry = DeviceTelemetry::fromJson(telemetryVal.toObject());
    }
    return dto;
}

QVector<DeviceStatus> DeviceStatus::listFromJson(const QJsonArray &arr) {
    QVector<DeviceStatus> result;
    result.reserve(arr.size());
    for (const QJsonValue &item : arr) {
        if (item.isObject()) {
            result.append(DeviceStatus::fromJson(item.toObject()));
        }
    }
    return result;
}

QJsonObject DeviceStatus::toJson() const {
    QJsonObject obj;
    obj.insert(QStringLiteral("deviceName"), deviceName);
    obj.insert(QStringLiteral("deviceType"), deviceType);
    obj.insert(QStringLiteral("topic"), topic);
    obj.insert(QStringLiteral("isOnline"), isOnline);
    obj.insert(QStringLiteral("lastSeenMs"), lastSeenMs.has_value() ? QJsonValue(*lastSeenMs) : QJsonValue::Null);
    obj.insert(QStringLiteral("telemetry"), telemetry.has_value() ? QJsonValue(telemetry->toJson()) : QJsonValue::Null);
    return obj;
}

RuleAction RuleAction::fromJson(const QJsonObject &obj) {
    RuleAction dto;
    dto.device = readOptionalString(obj, QStringLiteral("device"));
    dto.topic = readOptionalString(obj, QStringLiteral("topic"));
    dto.command = obj.value(QStringLiteral("command"));
    return dto;
}

QJsonObject RuleAction::toJson() const {
    QJsonObject obj;
    if (device.has_value()) {
        obj.insert(QStringLiteral("device"), *device);
    }
    if (topic.has_value()) {
        obj.insert(QStringLiteral("topic"), *topic);
    }
    if (!command.isUndefined() && !command.isNull()) {
        obj.insert(QStringLiteral("command"), command);
    }
    return obj;
}

RuleItem RuleItem::fromJson(const QJsonObject &obj) {
    RuleItem dto;
    dto.id = obj.value(QStringLiteral("id")).toString();
    dto.type = obj.value(QStringLiteral("type")).toString(QStringLiteral("point"));
    dto.enabled = obj.value(QStringLiteral("enabled")).toBool(true);

    dto.time = readOptionalString(obj, QStringLiteral("time"));
    dto.repeatDaily = readOptionalBool(obj, QStringLiteral("repeatDaily"));
    dto.timestamp = readOptionalInt64(obj, QStringLiteral("timestamp"));

    dto.startTime = readOptionalString(obj, QStringLiteral("startTime"));
    dto.endTime = readOptionalString(obj, QStringLiteral("endTime"));
    dto.maintainState = readOptionalBool(obj, QStringLiteral("maintainState"));
    dto.maintainIntervalSeconds = readOptionalInt(obj, QStringLiteral("maintainIntervalSeconds"));
    dto.startAction = readOptionalAction(obj, QStringLiteral("startAction"));
    dto.endAction = readOptionalAction(obj, QStringLiteral("endAction"));

    dto.intervalMs = readOptionalInt(obj, QStringLiteral("intervalMs"));
    dto.intervalSeconds = readOptionalInt(obj, QStringLiteral("intervalSeconds"));
    dto.maxTicks = readOptionalInt(obj, QStringLiteral("maxTicks"));
    if (!dto.maxTicks.has_value()) {
        dto.maxTicks = readOptionalInt(obj, QStringLiteral("maxRuns"));
    }

    dto.action = readOptionalAction(obj, QStringLiteral("action"));
    return dto;
}

QVector<RuleItem> RuleItem::listFromRulesResponse(const QJsonObject &obj) {
    QVector<RuleItem> result;
    const QJsonArray arr = obj.value(QStringLiteral("rules")).toArray();
    result.reserve(arr.size());
    for (const QJsonValue &item : arr) {
        if (item.isObject()) {
            result.append(RuleItem::fromJson(item.toObject()));
        }
    }
    return result;
}

QJsonObject RuleItem::toJson() const {
    QJsonObject obj;
    obj.insert(QStringLiteral("id"), id);
    obj.insert(QStringLiteral("type"), type);
    obj.insert(QStringLiteral("enabled"), enabled);

    if (time.has_value()) {
        obj.insert(QStringLiteral("time"), *time);
    }
    if (repeatDaily.has_value()) {
        obj.insert(QStringLiteral("repeatDaily"), *repeatDaily);
    }
    if (timestamp.has_value()) {
        obj.insert(QStringLiteral("timestamp"), *timestamp);
    }
    if (startTime.has_value()) {
        obj.insert(QStringLiteral("startTime"), *startTime);
    }
    if (endTime.has_value()) {
        obj.insert(QStringLiteral("endTime"), *endTime);
    }
    if (maintainState.has_value()) {
        obj.insert(QStringLiteral("maintainState"), *maintainState);
    }
    if (maintainIntervalSeconds.has_value()) {
        obj.insert(QStringLiteral("maintainIntervalSeconds"), *maintainIntervalSeconds);
    }
    if (startAction.has_value()) {
        obj.insert(QStringLiteral("startAction"), startAction->toJson());
    }
    if (endAction.has_value()) {
        obj.insert(QStringLiteral("endAction"), endAction->toJson());
    }
    if (intervalMs.has_value()) {
        obj.insert(QStringLiteral("intervalMs"), *intervalMs);
    }
    if (intervalSeconds.has_value()) {
        obj.insert(QStringLiteral("intervalSeconds"), *intervalSeconds);
    }
    if (maxTicks.has_value()) {
        obj.insert(QStringLiteral("maxTicks"), *maxTicks);
    }
    if (action.has_value()) {
        obj.insert(QStringLiteral("action"), action->toJson());
    }
    return obj;
}

HealthStatus HealthStatus::fromJson(const QJsonObject &obj) {
    HealthStatus dto;
    dto.status = obj.value(QStringLiteral("status")).toString();
    dto.service = obj.value(QStringLiteral("service")).toString();
    dto.version = obj.value(QStringLiteral("version")).toString();
    dto.mqttBroker = obj.value(QStringLiteral("mqtt_broker")).toString();
    dto.mqttConnected = obj.value(QStringLiteral("mqtt_connected")).toBool(false);
    dto.sqliteDbPath = obj.value(QStringLiteral("sqlite_db_path")).toString();
    dto.sqliteDbExists = obj.value(QStringLiteral("sqlite_db_exists")).toBool(false);
    dto.telemetryRecordsCount = obj.value(QStringLiteral("telemetry_records_count")).toInt(0);
    dto.rulesConfigPath = obj.value(QStringLiteral("rules_config_path")).toString();
    dto.registeredDevicesCount = obj.value(QStringLiteral("registered_devices_count")).toInt(0);

    const QJsonArray onlineArr = obj.value(QStringLiteral("online_devices")).toArray();
    dto.onlineDevices.reserve(onlineArr.size());
    for (const QJsonValue &val : onlineArr) {
        dto.onlineDevices.append(val.toString());
    }
    dto.activeWsClients = obj.value(QStringLiteral("active_ws_clients")).toInt(0);
    return dto;
}

QJsonObject CommandRequest::toJson() const {
    QJsonObject obj;
    obj.insert(QStringLiteral("command"), command);
    if (value.has_value()) {
        obj.insert(QStringLiteral("value"), *value);
    }
    if (mode.has_value()) {
        obj.insert(QStringLiteral("mode"), *mode);
    }
    return obj;
}

CommandResult CommandResult::fromJson(const QJsonObject &obj) {
    CommandResult dto;
    dto.status = obj.value(QStringLiteral("status")).toString();
    dto.deviceName = obj.value(QStringLiteral("deviceName")).toString();
    dto.topic = obj.value(QStringLiteral("topic")).toString();
    dto.payload = obj.value(QStringLiteral("payload")).toObject();
    return dto;
}

void registerDtoMetaTypes() {
    qRegisterMetaType<DeviceTelemetry>("smarthome::net::DeviceTelemetry");
    qRegisterMetaType<QVector<DeviceTelemetry>>("QVector<smarthome::net::DeviceTelemetry>");
    qRegisterMetaType<DeviceStatus>("smarthome::net::DeviceStatus");
    qRegisterMetaType<QVector<DeviceStatus>>("QVector<smarthome::net::DeviceStatus>");
    qRegisterMetaType<RuleItem>("smarthome::net::RuleItem");
    qRegisterMetaType<QVector<RuleItem>>("QVector<smarthome::net::RuleItem>");
    qRegisterMetaType<HealthStatus>("smarthome::net::HealthStatus");
    qRegisterMetaType<CommandResult>("smarthome::net::CommandResult");
}

} // namespace smarthome::net
