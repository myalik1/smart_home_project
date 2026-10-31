#pragma once

#include "SmartDevice.h"
#include "IMqttClient.h"
#include "DeviceTelemetry.h"
#include "TelemetryHistoryManager.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <mutex>
#include <chrono>
#include <optional>

class DeviceManager {
public:
    using StateListener = std::function<void(const std::string& deviceName, const std::string& stateJson)>;
    using TelemetryListener = std::function<void(const DeviceTelemetry& telemetry)>;

    explicit DeviceManager(IMqttClient* client = nullptr);
    virtual ~DeviceManager() = default;

    void setMqttClient(IMqttClient* client);
    IMqttClient* getMqttClient() const;

    bool registerDevice(std::shared_ptr<SmartDevice> device);
    bool unregisterDevice(const std::string& deviceName);

    bool hasDevice(const std::string& deviceName) const;
    std::shared_ptr<SmartDevice> getDevice(const std::string& deviceName) const;
    std::shared_ptr<SmartDevice> getDeviceByTopic(const std::string& topic) const;

    std::vector<std::shared_ptr<SmartDevice>> getAllDevices() const;
    size_t getDeviceCount() const;

    bool sendCommand(const std::string& deviceName, const std::string& commandJson);
    bool sendCommandToTopic(const std::string& topic, const std::string& commandJson);

    void publishAllStates();

    std::string getLatestState(const std::string& deviceName) const;
    std::unordered_map<std::string, std::string> getAllLatestStates() const;

    // Telemetry and active simulator monitoring (PPT-15)
    std::optional<DeviceTelemetry> getTelemetry(const std::string& deviceName) const;
    std::vector<DeviceTelemetry> getAllTelemetry() const;
    std::vector<DeviceTelemetry> collectActiveTelemetry(int timeoutSeconds = 10) const;

    bool isSimulatorActive(const std::string& deviceName, int timeoutSeconds = 10) const;
    std::vector<std::string> getActiveSimulators(int timeoutSeconds = 10) const;

    std::chrono::system_clock::time_point getLastSeenTime(const std::string& deviceName) const;

    // Database history integration (PPT-16)
    bool attachStorage(std::shared_ptr<IStorage> storage);
    std::shared_ptr<TelemetryHistoryManager> getHistoryManager() const;
    std::vector<DeviceTelemetry> getDeviceHistory(const std::string& deviceName, int limit = 100) const;

    void onStateUpdate(StateListener listener);
    void onTelemetryReceived(TelemetryListener listener);

    void handleIncomingMessage(const std::string& topic, const std::string& payload);

    // Enables wildcard listening (e.g. "home/#") to collect telemetry from external simulator apps
    void enableWildcardCollection(const std::string& rootTopic = "home/#");

private:
    IMqttClient* mqttClient_;
    mutable std::mutex mutex_;

    std::unordered_map<std::string, std::shared_ptr<SmartDevice>> devices_;
    std::unordered_map<std::string, std::shared_ptr<SmartDevice>> commandTopicMap_;
    std::unordered_map<std::string, std::string> stateTopicToNameMap_;
    std::unordered_map<std::string, std::string> latestStates_;

    std::unordered_map<std::string, DeviceTelemetry> latestTelemetry_;
    std::unordered_map<std::string, std::chrono::system_clock::time_point> lastSeenTime_;

    std::shared_ptr<TelemetryHistoryManager> historyManager_;

    std::vector<StateListener> stateListeners_;
    std::vector<TelemetryListener> telemetryListeners_;

    bool wildcardCollectionEnabled_ = false;
    std::string wildcardTopic_;

    struct LastStateMsgRecord {
        std::string payload;
        std::chrono::steady_clock::time_point timestamp;
    };
    std::unordered_map<std::string, LastStateMsgRecord> lastProcessedStateMap_;

    void setupDeviceSubscriptions(const std::shared_ptr<SmartDevice>& device);
    bool parseTelemetry(const std::string& payload, DeviceTelemetry& outTelemetry) const;
};
