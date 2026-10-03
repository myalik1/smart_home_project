#pragma once
#include "SmartDevice.h"
#include "json.hpp"
#include <string>

class SmartLamp : public SmartDevice {
public:
    SmartLamp(std::string name, std::string serial, std::string type);

    void onMessageReceived(const std::string& message) override;

private:
    bool isOn;
    int brightness;
};
