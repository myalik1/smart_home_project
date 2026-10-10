#pragma once
#include <string>
#include "IMessageSender.h"

// Abstract base class for all smart home devices
class SmartDevice {
public:
    virtual ~SmartDevice() = default;

    // Attaches a network sender to the device
    void setSender(IMessageSender* s);

    // Gathers device state and sends it via the attached sender
    virtual void publishState() = 0;

    // Processes incoming command strings
    virtual void onMessageReceived(const std::string& message) = 0;

protected:
    std::string deviceName;
    std::string serialNumber;
    std::string mqttTopic;
    std::string deviceType;

    // Pointer to the network communication interface
    IMessageSender* sender = nullptr;
};
