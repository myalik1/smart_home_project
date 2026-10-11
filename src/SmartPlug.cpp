#include "../include/SmartPlug.h"
#include "../include/json.hpp"
#include <iostream>
#include <algorithm>

using json = nlohmann::json;

SmartPlug::SmartPlug(std::string name, std::string serial, std::string type, std::string topic) {
    deviceName = name;
    serialNumber = serial;
    deviceType = type;
    mqttTopic = topic;
    isOn = false;
    currentPower = 0.0f;
}

void SmartPlug::publishState() {
    if (sender == nullptr) return;

    json data;
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        data["deviceName"] = deviceName;
        data["serialNumber"] = serialNumber;
        data["deviceType"] = deviceType;
        data["isOn"] = isOn;
        data["currentPower"] = currentPower;
    }

    sender->sendMessage(mqttTopic, data.dump());
}

void SmartPlug::onMessageReceived(const std::string& message) {
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
                    currentPower = 0.0f;
                    stateChanged = true;
                }
            }
            else if (command == "UPDATE_POWER" && incomingData.contains("value")) {
                if (isOn) {
                    float newVal = incomingData["value"];
                    currentPower = std::max(0.0f, newVal);
                    stateChanged = true;
                }
            }
        }

        if (stateChanged) {
            publishState();
        }

    } catch (const json::exception& e) {
        std::cerr << "[ERROR] SmartPlug JSON parse failed: " << e.what() << '\n';
    }
}
