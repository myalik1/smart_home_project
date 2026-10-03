#include "../include/SmartLamp.h"
#include "../include/json.hpp"
#include <iostream>

using json = nlohmann::json;

SmartLamp::SmartLamp(std::string name, std::string serial, std::string type) {
    deviceName = name;
    serialNumber = serial;
    deviceType = type;
    isOn = false;
}

void SmartLamp::onMessageReceived(const std::string& message) {
    json incomingData = json::parse(message);
    std::string command = incomingData["command"];

    if (command == "ON") {
        isOn = true;
        std::cout << "Lamp " << deviceName << " is ON (Brightness: " << brightness << "%)\n";
    }
    else if (command == "OFF") {
        isOn = false;
        std::cout << "Lamp " << deviceName << " is OFF\n";
    }
    else if (command == "SET_BRIGHTNESS") {
        brightness = incomingData["value"];
        std::cout << "Lamp " << deviceName << " changed brightness to " << brightness << "%\n";
    }
}
