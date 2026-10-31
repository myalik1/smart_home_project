#pragma once
#include "SmartDevice.h"
#include <string>
#include <mutex>

class SmartThermostat : public SmartDevice {
public:
    SmartThermostat(std::string name, std::string serial, std::string type, std::string topic,
                    std::string radCmdTopic = "home/radiator1/set");

    void onMessageReceived(const std::string& message) override;
    void publishState() override;

    float getCurrentTemperature();
    float getTargetTemperature();
    std::string getMode();
    void updateCurrentTemperature(float temp);
    int getRadiatorValveLevel();

private:
    void syncRadiatorValve();

    std::string mode; // "OFF", "HEAT", "COOL"
    float targetTemperature;
    float currentTemperature;

    std::string radiatorCommandTopic;
    int lastSentValveLevel;

    std::mutex stateMutex;
    int currentRadiatorValveLevel;
};
