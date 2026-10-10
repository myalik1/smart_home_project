#pragma once
#include "SmartDevice.h"
#include <string>

// Concrete implementation of a smart lamp device
class SmartLamp : public SmartDevice {
public:
    // Initializes the lamp with basic info and its target MQTT topic
    SmartLamp(std::string name, std::string serial, std::string type, std::string topic);

    // Handles ON, OFF, and SET_BRIGHTNESS JSON commands
    void onMessageReceived(const std::string& message) override;

    // Publishes the current power state and brightness level
    void publishState() override;

private:
    // Current power state
    bool isOn;

    // Current brightness level (0-100)
    int brightness;
};
