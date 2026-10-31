#include "../include/SmartPlug.h"
#include "../include/PlugEmulator.h"
#include "../include/MqttMessageSender.h"
#include <iostream>

int main() {
    MqttMessageSender sender("tcp://localhost:1883", "app_plug_client");
    SmartPlug plug("Plug1", "P-01", "Plug", "home/plug1");
    plug.setSender(&sender);

    PlugEmulator plgEmu(&plug);
    plgEmu.start();

    sender.connect();

    sender.subscribe("home/plug1/set", [&plug](const std::string&, const std::string& payload) {
        plug.onMessageReceived(payload);
    });

    plug.publishState();

    std::cout << "[Smart Plug Micro-app] is running.\n";
    std::cout << "Press Enter to stop and exit...\n";
    std::cin.get();

    plgEmu.stop();
    sender.disconnect();
    return 0;
}
