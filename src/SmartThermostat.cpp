#include "../include/SmartThermostat.h"
#include "../include/json.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>

using json = nlohmann::json;

SmartThermostat::SmartThermostat(std::string name, std::string serial, std::string type,
                                 std::string topic, std::string radCmdTopic) {
    deviceName = name;
    serialNumber = serial;
    deviceType = type;
    mqttTopic = topic;
    radiatorCommandTopic = radCmdTopic;

    mode = "OFF";
    targetTemperature = 22.0f;
    currentTemperature = 18.0f;
    lastSentValveLevel = -1;
}

void SmartThermostat::publishState() {
    if (sender == nullptr) return;

    json data;
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        data["deviceName"] = deviceName;
        data["serialNumber"] = serialNumber;
        data["deviceType"] = deviceType;
        data["mode"] = mode;
        data["targetTemperature"] = targetTemperature;
        data["currentTemperature"] = currentTemperature;
    }

    sender->sendMessage(mqttTopic, data.dump());
}

void SmartThermostat::syncRadiatorValve() {
    if (sender == nullptr || radiatorCommandTopic.empty()) return;

    int desiredValve = 0;
    bool shouldSend = false;

    {
        std::lock_guard<std::mutex> lock(stateMutex);

        if (mode == "HEAT" && currentTemperature < targetTemperature) {
            float diff = targetTemperature - currentTemperature;
            if (diff >= 3.0f) {
                desiredValve = 100;
            } else {
                desiredValve = std::clamp(static_cast<int>((diff / 3.0f) * 100.0f), 15, 100);
            }
        } else {
            desiredValve = 0;
        }

        if (desiredValve != lastSentValveLevel) {
            lastSentValveLevel = desiredValve;
            shouldSend = true;
        }
    }

    if (shouldSend) {
        json cmd;
        cmd["command"] = "SET_VALVE_LEVEL";
        cmd["value"] = desiredValve;
        sender->sendMessage(radiatorCommandTopic, cmd.dump());
    }
}

void SmartThermostat::onMessageReceived(const std::string& message) {
    try {
        json incomingData = json::parse(message);
        if (!incomingData.contains("command")) return;

        std::string command = incomingData["command"];
        bool stateChanged = false;

        {
            std::lock_guard<std::mutex> lock(stateMutex);

            if (command == "SET_MODE" && incomingData.contains("mode")) {
                std::string newMode = incomingData["mode"];
                if ((newMode == "HEAT" || newMode == "COOL" || newMode == "OFF") && mode != newMode) {
                    mode = newMode;
                    stateChanged = true;
                }
            }
            else if (command == "SET_TEMPERATURE" && incomingData.contains("value")) {
                float newTemp = std::clamp(incomingData["value"].get<float>(), 10.0f, 35.0f);
                if (std::abs(targetTemperature - newTemp) > 0.01f) {
                    targetTemperature = newTemp;
                    stateChanged = true;
                }
            }
            else if (command == "UPDATE_CURRENT_TEMP" && incomingData.contains("value")) {
                float newCurrent = incomingData["value"].get<float>();
                if (std::abs(currentTemperature - newCurrent) > 0.01f) {
                    currentTemperature = newCurrent;
                    stateChanged = true;
                }
            }
        }

        if (stateChanged) {
            publishState();
            syncRadiatorValve();
        }
    } catch (const json::exception& e) {
        std::cerr << "[ERROR] SmartThermostat JSON parse failed: " << e.what() << '\n';
    }
}

float SmartThermostat::getCurrentTemperature() {
    std::lock_guard<std::mutex> lock(stateMutex);
    return currentTemperature;
}

float SmartThermostat::getTargetTemperature() {
    std::lock_guard<std::mutex> lock(stateMutex);
    return targetTemperature;
}

std::string SmartThermostat::getMode() {
    std::lock_guard<std::mutex> lock(stateMutex);
    return mode;
}

void SmartThermostat::updateCurrentTemperature(float temp) {
    bool changed = false;
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        float rounded = std::round(temp * 10.0f) / 10.0f;
        if (std::abs(currentTemperature - rounded) >= 0.1f) {
            currentTemperature = rounded;
            changed = true;
        }
    }
    if (changed) {
        publishState();
        syncRadiatorValve();
    }
}
