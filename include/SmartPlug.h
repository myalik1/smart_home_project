#pragma once
#include "SmartDevice.h"
#include <string>
#include <mutex>

// Concrete implementation of a smart plug device with power monitoring
class SmartPlug : public SmartDevice {
public:
    SmartPlug(std::string name, std::string serial, std::string type, std::string topic);

    // Handles ON, OFF, and UPDATE_POWER JSON commands
    void onMessageReceived(const std::string& message) override;

    // Publishes current power state and power consumption (in Watts)
    void publishState() override;

private:
    // Current power state (true if electricity is flowing)
    bool isOn;

    // Current power consumption in Watts
    float currentPower;

    // Mutex to protect state variables from concurrent access
    std::mutex stateMutex;
};
