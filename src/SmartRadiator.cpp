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
    targetTemperature = 22.0f;
    currentTemperature = 20.0f;
    valveLevel = 0;
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
        data["targetTemperature"] = targetTemperature;
        data["currentTemperature"] = currentTemperature;
        data["valveLevel"] = valveLevel;
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

            if (command == "ON") {
                if (!isOn) {
                    isOn = true;
                    stateChanged = true;
                }
            }
            else if (command == "OFF") {
                if (isOn) {
                    isOn = false;
                    valveLevel = 0;
                    stateChanged = true;
                }
            }
            else if (command == "SET_TARGET_TEMP" && incomingData.contains("value")) {
                float newVal = incomingData["value"];
                targetTemperature = std::clamp(newVal, 5.0f, 30.0f);
                stateChanged = true;
            }
            else if (command == "UPDATE_CURRENT_TEMP" && incomingData.contains("value")) {
                currentTemperature = incomingData["value"];
                stateChanged = true;
            }
            else if (command == "SET_VALVE_LEVEL" && incomingData.contains("value")) {
                if (isOn) {
                    int newVal = incomingData["value"];
                    valveLevel = std::clamp(newVal, 0, 100);
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
