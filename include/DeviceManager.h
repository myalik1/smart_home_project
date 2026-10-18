#pragma once

#include "SmartDevice.h"
#include "IMqttClient.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <mutex>

class DeviceManager {
public:
    using StateListener = std::function<void(const std::string& deviceName, const std::string& stateJson)>;

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

    void onStateUpdate(StateListener listener);
    void handleIncomingMessage(const std::string& topic, const std::string& payload);

private:
    IMqttClient* mqttClient_;
    mutable std::mutex mutex_;

    std::unordered_map<std::string, std::shared_ptr<SmartDevice>> devices_;
    std::unordered_map<std::string, std::shared_ptr<SmartDevice>> commandTopicMap_;
    std::unordered_map<std::string, std::string> stateTopicToNameMap_;
    std::unordered_map<std::string, std::string> latestStates_;
    std::vector<StateListener> stateListeners_;

    void setupDeviceSubscriptions(const std::shared_ptr<SmartDevice>& device);
};

