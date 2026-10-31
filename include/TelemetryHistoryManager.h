#pragma once

#include "IStorage.h"
#include "DeviceTelemetry.h"

#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <chrono>

class TelemetryHistoryManager {
public:
    explicit TelemetryHistoryManager(std::shared_ptr<IStorage> storage);
    virtual ~TelemetryHistoryManager() = default;

    // Creates the telemetry_history table and indexes if not exists
    bool initSchema();

    // Saves a single telemetry record into the database
    bool saveTelemetry(const DeviceTelemetry& telemetry);

    // Queries history for a specific device, sorted by timestamp descending
    std::vector<DeviceTelemetry> getHistory(const std::string& deviceName, int limit = 100) const;

    // Queries all history across all devices, sorted by timestamp descending
    std::vector<DeviceTelemetry> getAllHistory(int limit = 100) const;

    // Queries history within a time range (timestamps in milliseconds)
    std::vector<DeviceTelemetry> getHistoryRange(const std::string& deviceName,
                                                 int64_t startTimestampMs,
                                                 int64_t endTimestampMs) const;

    // Returns the most recently stored record for a device
    std::optional<DeviceTelemetry> getLatestRecorded(const std::string& deviceName) const;

    // Returns total number of stored records (or for a specific device)
    size_t getHistoryCount(const std::string& deviceName = "") const;

    // Clears stored telemetry history
    bool clearHistory(const std::string& deviceName = "");

    std::shared_ptr<IStorage> getStorage() const;

private:
    std::shared_ptr<IStorage> storage_;

    DeviceTelemetry rowToTelemetry(const IStorage::Row& row) const;
    static int64_t timePointToMs(const std::chrono::system_clock::time_point& tp);
    static std::chrono::system_clock::time_point msToTimePoint(int64_t ms);
};
