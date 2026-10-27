#include "../include/DeviceManager.h"
#include "../include/json.hpp"
#include <iostream>
#include <algorithm>

using json = nlohmann::json;

DeviceManager::DeviceManager(IMqttClient* client)
    : mqttClient_(client) {}

void DeviceManager::setMqttClient(IMqttClient* client) {
    std::lock_guard<std::mutex> lock(mutex_);
    mqttClient_ = client;

    if (mqttClient_) {
        for (const auto& [name, device] : devices_) {
            setupDeviceSubscriptions(device);
        }
    }
}

IMqttClient* DeviceManager::getMqttClient() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return mqttClient_;
}

bool DeviceManager::registerDevice(std::shared_ptr<SmartDevice> device) {
    if (!device) {
        return false;
    }

    const std::string name = device->getName();
    if (name.empty()) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (devices_.find(name) != devices_.end()) {
            return false;
        }

        devices_[name] = device;

        const std::string baseTopic = device->getTopic();
        const std::string cmdTopic = baseTopic + "/set";

        commandTopicMap_[cmdTopic] = device;
        stateTopicToNameMap_[baseTopic] = name;

        if (mqttClient_) {
            setupDeviceSubscriptions(device);
        }
    }

    return true;
}

bool DeviceManager::unregisterDevice(const std::string& deviceName) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = devices_.find(deviceName);
    if (it == devices_.end()) {
        return false;
    }

    auto device = it->second;
    const std::string baseTopic = device->getTopic();
    const std::string cmdTopic = baseTopic + "/set";

    commandTopicMap_.erase(cmdTopic);
    stateTopicToNameMap_.erase(baseTopic);
    latestStates_.erase(deviceName);
    latestTelemetry_.erase(deviceName);
    lastSeenTime_.erase(deviceName);
    devices_.erase(it);

    return true;
}

bool DeviceManager::hasDevice(const std::string& deviceName) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return devices_.find(deviceName) != devices_.end();
}

std::shared_ptr<SmartDevice> DeviceManager::getDevice(const std::string& deviceName) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = devices_.find(deviceName);
    if (it != devices_.end()) {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<SmartDevice> DeviceManager::getDeviceByTopic(const std::string& topic) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto itCmd = commandTopicMap_.find(topic);
    if (itCmd != commandTopicMap_.end()) {
        return itCmd->second;
    }

    auto itState = stateTopicToNameMap_.find(topic);
    if (itState != stateTopicToNameMap_.end()) {
        auto itDev = devices_.find(itState->second);
        if (itDev != devices_.end()) {
            return itDev->second;
        }
    }

    return nullptr;
}

std::vector<std::shared_ptr<SmartDevice>> DeviceManager::getAllDevices() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::shared_ptr<SmartDevice>> result;
    result.reserve(devices_.size());
    for (const auto& [name, dev] : devices_) {
        result.push_back(dev);
    }
    return result;
}

size_t DeviceManager::getDeviceCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return devices_.size();
}

bool DeviceManager::sendCommand(const std::string& deviceName, const std::string& commandJson) {
    std::string cmdTopic;
    IMqttClient* client = nullptr;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = devices_.find(deviceName);
        if (it == devices_.end()) {
            return false;
        }

        cmdTopic = it->second->getTopic() + "/set";
        client = mqttClient_;
    }

    if (!client) {
        return false;
    }

    client->sendMessage(cmdTopic, commandJson);
    return true;
}

bool DeviceManager::sendCommandToTopic(const std::string& topic, const std::string& commandJson) {
    IMqttClient* client = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        client = mqttClient_;
    }

    if (!client) {
        return false;
    }

    client->sendMessage(topic, commandJson);
    return true;
}

void DeviceManager::publishAllStates() {
    std::vector<std::shared_ptr<SmartDevice>> devicesCopy;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        devicesCopy.reserve(devices_.size());
        for (const auto& [name, dev] : devices_) {
            devicesCopy.push_back(dev);
        }
    }

    for (const auto& dev : devicesCopy) {
        if (dev) {
            dev->publishState();
        }
    }
}

std::string DeviceManager::getLatestState(const std::string& deviceName) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = latestStates_.find(deviceName);
    if (it != latestStates_.end()) {
        return it->second;
    }
    return "";
}

std::unordered_map<std::string, std::string> DeviceManager::getAllLatestStates() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return latestStates_;
}

std::optional<DeviceTelemetry> DeviceManager::getTelemetry(const std::string& deviceName) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = latestTelemetry_.find(deviceName);
    if (it != latestTelemetry_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<DeviceTelemetry> DeviceManager::getAllTelemetry() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<DeviceTelemetry> list;
    list.reserve(latestTelemetry_.size());
    for (const auto& [name, t] : latestTelemetry_) {
        list.push_back(t);
    }
    return list;
}

std::vector<DeviceTelemetry> DeviceManager::collectActiveTelemetry(int timeoutSeconds) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto now = std::chrono::system_clock::now();
    const auto timeout = std::chrono::seconds(timeoutSeconds);

    std::vector<DeviceTelemetry> active;
    for (const auto& [name, telemetry] : latestTelemetry_) {
        auto itTime = lastSeenTime_.find(name);
        if (itTime != lastSeenTime_.end()) {
            if ((now - itTime->second) <= timeout) {
                active.push_back(telemetry);
            }
        }
    }
    return active;
}

bool DeviceManager::isSimulatorActive(const std::string& deviceName, int timeoutSeconds) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = lastSeenTime_.find(deviceName);
    if (it == lastSeenTime_.end()) {
        return false;
    }

    const auto now = std::chrono::system_clock::now();
    return (now - it->second) <= std::chrono::seconds(timeoutSeconds);
}

std::vector<std::string> DeviceManager::getActiveSimulators(int timeoutSeconds) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto now = std::chrono::system_clock::now();
    const auto timeout = std::chrono::seconds(timeoutSeconds);

    std::vector<std::string> activeNames;
    for (const auto& [name, seenTime] : lastSeenTime_) {
        if ((now - seenTime) <= timeout) {
            activeNames.push_back(name);
        }
    }
    return activeNames;
}

std::chrono::system_clock::time_point DeviceManager::getLastSeenTime(const std::string& deviceName) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = lastSeenTime_.find(deviceName);
    if (it != lastSeenTime_.end()) {
        return it->second;
    }
    return std::chrono::system_clock::time_point{};
}

void DeviceManager::onStateUpdate(StateListener listener) {
    std::lock_guard<std::mutex> lock(mutex_);
    stateListeners_.push_back(std::move(listener));
}

void DeviceManager::onTelemetryReceived(TelemetryListener listener) {
    std::lock_guard<std::mutex> lock(mutex_);
    telemetryListeners_.push_back(std::move(listener));
}

void DeviceManager::enableWildcardCollection(const std::string& rootTopic) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!mqttClient_) {
        return;
    }

    mqttClient_->subscribe(rootTopic, [this](const std::string& topic, const std::string& payload) {
        this->handleIncomingMessage(topic, payload);
    });
}

bool DeviceManager::parseTelemetry(const std::string& payload, DeviceTelemetry& outTelemetry) const {
    try {
        json j = json::parse(payload);
        if (!j.is_object()) {
            return false;
        }

        outTelemetry.timestamp = std::chrono::system_clock::now();
        outTelemetry.rawJson = payload;

        if (j.contains("deviceName") && j["deviceName"].is_string()) {
            outTelemetry.deviceName = j["deviceName"].get<std::string>();
        }
        if (j.contains("deviceType") && j["deviceType"].is_string()) {
            outTelemetry.deviceType = j["deviceType"].get<std::string>();
        }
        if (j.contains("serialNumber") && j["serialNumber"].is_string()) {
            outTelemetry.serialNumber = j["serialNumber"].get<std::string>();
        }

        if (j.contains("isOn") && j["isOn"].is_boolean()) {
            outTelemetry.isOn = j["isOn"].get<bool>();
            outTelemetry.numericMetrics["isOn"] = outTelemetry.isOn ? 1.0 : 0.0;
        }
        if (j.contains("currentTemperature") && j["currentTemperature"].is_number()) {
            outTelemetry.currentTemperature = j["currentTemperature"].get<float>();
            outTelemetry.numericMetrics["currentTemperature"] = outTelemetry.currentTemperature;
        }
        if (j.contains("targetTemperature") && j["targetTemperature"].is_number()) {
            outTelemetry.targetTemperature = j["targetTemperature"].get<float>();
            outTelemetry.numericMetrics["targetTemperature"] = outTelemetry.targetTemperature;
        }
        if (j.contains("currentHumidity") && j["currentHumidity"].is_number()) {
            outTelemetry.currentHumidity = j["currentHumidity"].get<float>();
            outTelemetry.numericMetrics["currentHumidity"] = outTelemetry.currentHumidity;
        }
        if (j.contains("currentPower") && j["currentPower"].is_number()) {
            outTelemetry.currentPower = j["currentPower"].get<float>();
            outTelemetry.numericMetrics["currentPower"] = outTelemetry.currentPower;
        }
        if (j.contains("brightness") && j["brightness"].is_number()) {
            outTelemetry.brightness = j["brightness"].get<int>();
            outTelemetry.numericMetrics["brightness"] = outTelemetry.brightness;
        }
        if (j.contains("valveLevel") && j["valveLevel"].is_number()) {
            outTelemetry.valveLevel = j["valveLevel"].get<int>();
            outTelemetry.numericMetrics["valveLevel"] = outTelemetry.valveLevel;
        }
        if (j.contains("targetValveLevel") && j["targetValveLevel"].is_number()) {
            outTelemetry.targetValveLevel = j["targetValveLevel"].get<int>();
            outTelemetry.numericMetrics["targetValveLevel"] = outTelemetry.targetValveLevel;
        }
        if (j.contains("currentValveLevel") && j["currentValveLevel"].is_number()) {
            outTelemetry.currentValveLevel = j["currentValveLevel"].get<int>();
            outTelemetry.numericMetrics["currentValveLevel"] = outTelemetry.currentValveLevel;
        }
        if (j.contains("mode") && j["mode"].is_string()) {
            outTelemetry.mode = j["mode"].get<std::string>();
            outTelemetry.stringMetrics["mode"] = outTelemetry.mode;
        }

        // Store any other arbitrary keys
        for (auto it = j.begin(); it != j.end(); ++it) {
            const std::string& key = it.key();
            if (it.value().is_number()) {
                outTelemetry.numericMetrics[key] = it.value().get<double>();
            } else if (it.value().is_string()) {
                outTelemetry.stringMetrics[key] = it.value().get<std::string>();
            }
        }

        return !outTelemetry.deviceName.empty();
    } catch (...) {
        return false;
    }
}

void DeviceManager::handleIncomingMessage(const std::string& topic, const std::string& payload) {
    // 1. Check if it's a command message targeted to /set topic
    const bool isCommandTopic = (topic.size() >= 4 && topic.substr(topic.size() - 4) == "/set");

    std::shared_ptr<SmartDevice> targetDevice = nullptr;
    std::string matchedDeviceName;
    DeviceTelemetry parsedTelemetry;
    bool hasValidTelemetry = false;

    std::vector<StateListener> stateListenersCopy;
    std::vector<TelemetryListener> telemetryListenersCopy;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (isCommandTopic) {
            auto itCmd = commandTopicMap_.find(topic);
            if (itCmd != commandTopicMap_.end()) {
                targetDevice = itCmd->second;
            }
        } else {
            // It's a state / telemetry message
            auto itState = stateTopicToNameMap_.find(topic);
            if (itState != stateTopicToNameMap_.end()) {
                matchedDeviceName = itState->second;
            }

            hasValidTelemetry = parseTelemetry(payload, parsedTelemetry);
            if (hasValidTelemetry) {
                std::string devName = parsedTelemetry.deviceName;
                if (devName.empty()) {
                    devName = matchedDeviceName;
                    parsedTelemetry.deviceName = devName;
                }

                if (!devName.empty()) {
                    latestStates_[devName] = payload;
                    latestTelemetry_[devName] = parsedTelemetry;
                    lastSeenTime_[devName] = parsedTelemetry.timestamp;
                    matchedDeviceName = devName;
                }
            } else if (!matchedDeviceName.empty()) {
                latestStates_[matchedDeviceName] = payload;
            }

            stateListenersCopy = stateListeners_;
            telemetryListenersCopy = telemetryListeners_;
        }
    }

    if (targetDevice) {
        targetDevice->onMessageReceived(payload);
    }

    if (!isCommandTopic && !matchedDeviceName.empty()) {
        for (const auto& listener : stateListenersCopy) {
            if (listener) {
                listener(matchedDeviceName, payload);
            }
        }

        if (hasValidTelemetry) {
            for (const auto& listener : telemetryListenersCopy) {
                if (listener) {
                    listener(parsedTelemetry);
                }
            }
        }
    }
}

void DeviceManager::setupDeviceSubscriptions(const std::shared_ptr<SmartDevice>& device) {
    if (!mqttClient_ || !device) {
        return;
    }

    device->setSender(mqttClient_);

    const std::string baseTopic = device->getTopic();
    const std::string cmdTopic = baseTopic + "/set";

    mqttClient_->subscribe(cmdTopic, [this](const std::string& topic, const std::string& payload) {
        this->handleIncomingMessage(topic, payload);
    });

    mqttClient_->subscribe(baseTopic, [this](const std::string& topic, const std::string& payload) {
        this->handleIncomingMessage(topic, payload);
    });
}
