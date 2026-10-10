#pragma once
#include "SmartDevice.h"
#include <string>
#include <mutex>

// Concrete implementation of a smart thermostat device
class SmartThermostat : public SmartDevice {
public:
    SmartThermostat(std::string name, std::string serial, std::string type, std::string topic);

    // Handles SET_TEMPERATURE and SET_MODE JSON commands
    void onMessageReceived(const std::string& message) override;

    // Publishes current/target temperatures and HVAC mode
    void publishState() override;

private:
    // Target temperature set by the user
    float targetTemperature;

    // Current ambient temperature measured by sensor
    float currentTemperature;

    // Current operating mode (e.g., "OFF", "HEAT", "COOL")
    std::string mode;

    // Mutex to protect state variables from concurrent access
    std::mutex stateMutex;
};
