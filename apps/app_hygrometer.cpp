#include "../include/SmartHygrometer.h"
#include "../include/HygrometerEmulator.h"
#include "../include/MqttMessageSender.h"
#include <iostream>

int main() {
    MqttMessageSender sender("tcp://localhost:1883", "app_hygro_client");
    SmartHygrometer hygrometer("Hygro1", "H-01", "Hygrometer", "home/hygro1");
    hygrometer.setSender(&sender);

    HygrometerEmulator hygroEmu(&hygrometer, 45.0f);
    hygroEmu.start();

    sender.connect();

    sender.subscribe("home/hygro1/set", [&hygrometer](const std::string&, const std::string& payload) {
        hygrometer.onMessageReceived(payload);
    });

    hygrometer.publishState();

    std::cout << "[Hygrometer Micro-app] is running.\n";
    std::cout << "Press Enter to stop and exit...\n";
    std::cin.get();

    hygroEmu.stop();
    sender.disconnect();
    return 0;
}
