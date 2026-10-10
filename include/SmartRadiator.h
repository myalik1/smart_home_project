#pragma once
#include "SmartDevice.h"
#include <string>
#include <mutex>

// Concrete implementation of a smart radiator (heating battery)
class SmartRadiator : public SmartDevice {
public:
    SmartRadiator(std::string name, std::string serial, std::string type, std::string topic);

    // Handles ON, OFF, SET_TARGET_TEMP, UPDATE_CURRENT_TEMP, and SET_VALVE_LEVEL commands
    void onMessageReceived(const std::string& message) override;

    // Publishes power state, temperatures, and valve level
    void publishState() override;

private:
    // Current power state
    bool isOn;

    // Target temperature set by the user
    float targetTemperature;

    // Current ambient temperature around the radiator
    float currentTemperature;

    // Radiator valve opening level in percentage (0-100)
    int valveLevel;

    // Mutex to protect state variables from concurrent access
    std::mutex stateMutex;
};
