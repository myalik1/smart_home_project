#include "../include/SmartLamp.h"
#include "../include/json.hpp"
#include <iostream>
#include <algorithm>

using json = nlohmann::json;

SmartLamp::SmartLamp(std::string name, std::string serial, std::string type, std::string topic) {
    deviceName = name;
    serialNumber = serial;
    deviceType = type;
    mqttTopic = topic;
    isOn = false;
    brightness = 100;
}

void SmartLamp::publishState() {

    if (sender == nullptr) return;

    json data;
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        data["deviceName"] = deviceName;
        data["serialNumber"] = serialNumber;
        data["deviceType"] = deviceType;
        data["isOn"] = isOn;
        data["brightness"] = brightness;
    }

    sender->sendMessage(mqttTopic, data.dump());
}

void SmartLamp::onMessageReceived(const std::string& message) {
    try {
        json incomingData = json::parse(message);

        if (!incomingData.contains("command")) return;

        std::string command = incomingData["command"];
        bool stateChanged = false;

        {
            std::lock_guard<std::mutex> lock(stateMutex);

            if (command == "ON") {
                isOn = true;
                stateChanged = true;
            }
            else if (command == "OFF") {
                isOn = false;
                stateChanged = true;
            }
            else if (command == "SET_BRIGHTNESS" && incomingData.contains("value")) {
                int newVal = incomingData["value"];
                brightness = std::clamp(newVal, 0, 100);
                stateChanged = true;
            }
        }

        if (stateChanged) {
            publishState();
        }

    } catch (const json::exception& e) {
            std::cerr << "[ERROR] JSON parse failed: " << e.what() << '\n';
    }
}
