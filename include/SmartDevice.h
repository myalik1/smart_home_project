#pragma once
#include <string>

class SmartDevice {
public:
    virtual ~SmartDevice() = default;
    virtual void connectToNetwork();
    virtual void publishState();
    virtual void onMessageReceived(const std::string& message) = 0;
protected:
    std::string deviceName;
    std::string serialNumber;
    std::string mqttTopic;
    std::string deviceType;
};
