#include "../include/SmartLamp.h"
#include "../include/MqttMessageSender.h"
#include <iostream>

int main() {
    MqttMessageSender sender("tcp://localhost:1883", "app_lamp_client");
    SmartLamp lamp("Lamp1", "L-01", "Lamp", "home/lamp1");
    lamp.setSender(&sender);

    sender.connect();

    sender.subscribe("home/lamp1/set", [&lamp](const std::string&, const std::string& payload) {
        lamp.onMessageReceived(payload);
    });

    lamp.publishState();

    std::cout << "[Smart Lamp Micro-app] is running.\n";
    std::cout << "Press Enter to stop and exit...\n";
    std::cin.get();

    sender.disconnect();
    return 0;
}
