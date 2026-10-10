#include "../include/SmartLamp.h"
#include "../include/json.hpp"

using json = nlohmann::json;

SmartLamp::SmartLamp(std::string name, std::string serial, std::string type, std::string topic) {
    deviceName = name;
    serialNumber = serial;
    deviceType = type;
    mqttTopic = topic;
    isOn = false;
    brightness = 100;
}

void SmartLamp::publishState() {

    if (sender == nullptr) return;

    json data;
    data["deviceName"] = deviceName;
    data["serialNumber"] = serialNumber;
    data["deviceType"] = deviceType;
    data["isOn"] = isOn;
    data["brightness"] = brightness;

    sender->sendMessage(mqttTopic, data.dump());
}

void SmartLamp::onMessageReceived(const std::string& message) {
    json incomingData = json::parse(message);
    std::string command = incomingData["command"];

    if (command == "ON") {
        isOn = true;
    }
    else if (command == "OFF") {
        isOn = false;
    }
    else if (command == "SET_BRIGHTNESS") {
        brightness = incomingData["value"];
    }

    publishState();
}
