#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include "include/SmartLamp.h"
#include "include/MqttMessageSender.h"

int main() {
    const std::string BROKER = "tcp://localhost:1883";

    const std::string CLIENT_ID = "smart_home_app";

    MqttMessageSender sender(BROKER, CLIENT_ID);

    SmartLamp lamp("Lamp1", "123456", "Lamp", "home/lamp1");

    lamp.setSender(&sender);

    sender.connect();

    sender.subscribe("home/lamp1/set",
        [&lamp](const std::string& topic, const std::string& payload) {
            std::cout << "[main] Команда для " << topic << ": " << payload << std::endl;
            lamp.onMessageReceived(payload);
        });

    lamp.publishState();

    std::cout << "\nЛампа готова. Отправь команду в другом терминале:\n";
    std::cout << "  mosquitto_pub -h localhost -t home/lamp1/set -m '{\"command\": \"ON\"}'\n";
    std::cout << "  mosquitto_pub -h localhost -t home/lamp1/set -m '{\"command\": \"SET_BRIGHTNESS\", \"value\": 75}'\n";
    std::cout << "\nCtrl+C для выхода.\n" << std::endl;

    std::this_thread::sleep_for(std::chrono::hours(24));

    sender.disconnect();
    return 0;
}