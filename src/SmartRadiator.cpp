#include "../include/SmartRadiator.h"
#include "../include/json.hpp"
#include <iostream>
#include <algorithm>

using json = nlohmann::json;

SmartRadiator::SmartRadiator(std::string name, std::string serial, std::string type, std::string topic) {
    deviceName = name;
    serialNumber = serial;
    deviceType = type;
    mqttTopic = topic;
    isOn = false;
    targetValveLevel = 0;
    currentValveLevel = 0;
}

void SmartRadiator::publishState() {
    if (sender == nullptr) return;

    json data;
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        data["deviceName"] = deviceName;
        data["serialNumber"] = serialNumber;
        data["deviceType"] = deviceType;
        data["isOn"] = isOn;
        data["targetValveLevel"] = targetValveLevel;
        data["currentValveLevel"] = currentValveLevel;
    }

    sender->sendMessage(mqttTopic, data.dump());
}

void SmartRadiator::onMessageReceived(const std::string& message) {
    try {
        json incomingData = json::parse(message);
        if (!incomingData.contains("command")) return;

        std::string command = incomingData["command"];
        bool stateChanged = false;

        {
            std::lock_guard<std::mutex> lock(stateMutex);

            if (command == "ON" && !isOn) {
                isOn = true;
                stateChanged = true;
            }
            else if (command == "OFF" && (isOn || targetValveLevel != 0)) {
                isOn = false;
                targetValveLevel = 0;
                stateChanged = true;
            }
            else if (command == "SET_VALVE_LEVEL" && incomingData.contains("value")) {
                int newLevel = std::clamp(incomingData["value"].get<int>(), 0, 100);
                bool newPowerState = (newLevel > 0);

                if (targetValveLevel != newLevel || isOn != newPowerState) {
                    targetValveLevel = newLevel;
                    isOn = newPowerState;
                    stateChanged = true;
                }
            }
        }

        if (stateChanged) {
            publishState();
        }
    } catch (const json::exception& e) {
        std::cerr << "[ERROR] SmartRadiator JSON parse failed: " << e.what() << '\n';
    }
}

bool SmartRadiator::getIsOn() {
    std::lock_guard<std::mutex> lock(stateMutex);
    return isOn;
}

int SmartRadiator::getTargetValveLevel() {
    std::lock_guard<std::mutex> lock(stateMutex);
    return targetValveLevel;
}

int SmartRadiator::getCurrentValveLevel() {
    std::lock_guard<std::mutex> lock(stateMutex);
    return currentValveLevel;
}

void SmartRadiator::setCurrentValveLevel(int level) {
    std::lock_guard<std::mutex> lock(stateMutex);
    currentValveLevel = std::clamp(level, 0, 100);
}
