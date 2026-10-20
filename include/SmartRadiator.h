#pragma once
#include "SmartDevice.h"
#include <string>
#include <mutex>

class SmartRadiator : public SmartDevice {
public:
    SmartRadiator(std::string name, std::string serial, std::string type, std::string topic);

    void onMessageReceived(const std::string& message) override;
    void publishState() override;

    bool getIsOn();
    int getTargetValveLevel();
    int getCurrentValveLevel();
    void setCurrentValveLevel(int level);

private:
    bool isOn;
    int targetValveLevel;
    int currentValveLevel;
    std::mutex stateMutex;
};
