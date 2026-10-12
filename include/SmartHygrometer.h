#pragma once
#include "SmartDevice.h"
#include <string>
#include <mutex>

// Concrete implementation of a smart hygrometer (humidity sensor)
class SmartHygrometer : public SmartDevice {
public:
    SmartHygrometer(std::string name, std::string serial, std::string type, std::string topic);

    // Handles UPDATE_HUMIDITY JSON command
    void onMessageReceived(const std::string& message) override;

    // Publishes current humidity level
    void publishState() override;

private:
    // Current humidity level in percentage (0-100)
    float currentHumidity;

    // Mutex to protect state variables from concurrent access
    std::mutex stateMutex;
};
