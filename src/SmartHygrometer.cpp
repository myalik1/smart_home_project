#include "../include/SmartHygrometer.h"
#include "../include/json.hpp"
#include <iostream>
#include <algorithm>

using json = nlohmann::json;

SmartHygrometer::SmartHygrometer(std::string name, std::string serial, std::string type, std::string topic) {
    deviceName = name;
    serialNumber = serial;
    deviceType = type;
    mqttTopic = topic;
    currentHumidity = 50.0f;
}

void SmartHygrometer::publishState() {
    if (sender == nullptr) return;

    json data;
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        data["deviceName"] = deviceName;
        data["serialNumber"] = serialNumber;
        data["deviceType"] = deviceType;
        data["currentHumidity"] = currentHumidity;
    }

    sender->sendMessage(mqttTopic, data.dump());
}

void SmartHygrometer::onMessageReceived(const std::string& message) {
    try {
        json incomingData = json::parse(message);
        if (!incomingData.contains("command")) return;

        std::string command = incomingData["command"];
        bool stateChanged = false;

        {
            std::lock_guard<std::mutex> lock(stateMutex);

            if (command == "UPDATE_HUMIDITY" && incomingData.contains("value")) {
                float newVal = incomingData["value"];
                float validatedHumidity = std::clamp(newVal, 0.0f, 100.0f);

                if (currentHumidity != validatedHumidity) {
                    currentHumidity = validatedHumidity;
                    stateChanged = true;
                }
            }
        }

        if (stateChanged) {
            publishState();
        }

    } catch (const json::exception& e) {
        std::cerr << "[ERROR] SmartHygrometer JSON parse failed: " << e.what() << '\n';
    }
}
