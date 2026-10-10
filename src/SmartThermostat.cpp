#include "../include/SmartThermostat.h"
#include "../include/json.hpp"
#include <iostream>
#include <algorithm>

using json = nlohmann::json;

SmartThermostat::SmartThermostat(std::string name, std::string serial, std::string type, std::string topic) {
    deviceName = name;
    serialNumber = serial;
    deviceType = type;
    mqttTopic = topic;
    targetTemperature = 22.0f;
    currentTemperature = 22.0f;
    mode = "OFF";
}

void SmartThermostat::publishState() {
    if (sender == nullptr) return;

    json data;
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        data["deviceName"] = deviceName;
        data["serialNumber"] = serialNumber;
        data["deviceType"] = deviceType;
        data["targetTemperature"] = targetTemperature;
        data["currentTemperature"] = currentTemperature;
        data["mode"] = mode;
    }

    sender->sendMessage(mqttTopic, data.dump());
}

void SmartThermostat::onMessageReceived(const std::string& message) {
    try {
        json incomingData = json::parse(message);
        if (!incomingData.contains("command")) return;

        std::string command = incomingData["command"];
        bool stateChanged = false;

        {
            std::lock_guard<std::mutex> lock(stateMutex);

            if (command == "SET_TEMPERATURE" && incomingData.contains("value")) {
                float newVal = incomingData["value"];
                targetTemperature = std::clamp(newVal, 10.0f, 35.0f);
                stateChanged = true;
            }
            else if (command == "SET_MODE" && incomingData.contains("mode")) {
                std::string newMode = incomingData["mode"];
                if (newMode == "OFF" || newMode == "HEAT" || newMode == "COOL") {
                    mode = newMode;
                    stateChanged = true;
                }
            }
            else if (command == "UPDATE_CURRENT_TEMP" && incomingData.contains("value")) {
                currentTemperature = incomingData["value"];
                stateChanged = true;
            }
        }

        if (stateChanged) {
            publishState();
        }

    } catch (const json::exception& e) {
        std::cerr << "[ERROR] Thermostat JSON parse failed: " << e.what() << '\n';
    }
}
