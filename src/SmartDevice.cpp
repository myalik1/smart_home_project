#include "../include/SmartDevice.h"
#include "../include/json.hpp"
#include <iostream>

using json = nlohmann::json;

void SmartDevice::publishState() {
    json data;
    data["deviceName"] = deviceName;
    data["serialNumber"] = serialNumber;
    data["deviceType"] = deviceType;

    std::string payload = data.dump();
    std::cout << "Publishing: " << payload << '\n';
}

void SmartDevice::connectToNetwork() {
    std::cout << "Connecting " << deviceName << " to MQTT broker...\n";
}
