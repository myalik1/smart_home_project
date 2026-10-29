#include "../include/SmartRadiator.h"
#include "../include/RadiatorEmulator.h"
#include "../include/MqttMessageSender.h"
#include <iostream>

int main() {
    MqttMessageSender sender("tcp://localhost:1883", "app_radiator_client");
    SmartRadiator radiator("Rad1", "R-01", "Radiator", "home/radiator1");
    radiator.setSender(&sender);

    RadiatorEmulator radEmu(&radiator, 10);
    radEmu.start();

    sender.connect();

    sender.subscribe("home/radiator1/set", [&radiator](const std::string&, const std::string& payload) {
        radiator.onMessageReceived(payload);
    });

    radiator.publishState();

    std::cout << "[Radiator Micro-app] is running.\n";
    std::cout << "Press Enter to stop and exit...\n";
    std::cin.get();

    radEmu.stop();
    sender.disconnect();
    return 0;
}
