#include "../include/DeviceManager.h"
#include <iostream>

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

void DeviceManager::onStateUpdate(StateListener listener) {
    std::lock_guard<std::mutex> lock(mutex_);
    stateListeners_.push_back(std::move(listener));
}

void DeviceManager::handleIncomingMessage(const std::string& topic, const std::string& payload) {
    std::shared_ptr<SmartDevice> targetDevice = nullptr;
    std::string matchedDeviceName;
    std::vector<StateListener> listenersCopy;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto itCmd = commandTopicMap_.find(topic);
        if (itCmd != commandTopicMap_.end()) {
            targetDevice = itCmd->second;
        }

        auto itState = stateTopicToNameMap_.find(topic);
        if (itState != stateTopicToNameMap_.end()) {
            matchedDeviceName = itState->second;
            latestStates_[matchedDeviceName] = payload;
            listenersCopy = stateListeners_;
        }
    }

    if (targetDevice) {
        targetDevice->onMessageReceived(payload);
    }

    if (!matchedDeviceName.empty()) {
        for (const auto& listener : listenersCopy) {
            if (listener) {
                listener(matchedDeviceName, payload);
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
