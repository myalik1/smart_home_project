#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include "include/SmartLamp.h"
#include "include/SmartThermostat.h"
#include "include/SmartPlug.h"
#include "include/SmartHygrometer.h"
#include "include/SmartRadiator.h"
#include "include/MqttMessageSender.h"
#include "include/PlugEmulator.h"
#include "include/ThermostatEmulator.h"
#include "include/RadiatorEmulator.h"
#include "include/HygrometerEmulator.h"

int main() {
    const std::string BROKER = "tcp://localhost:1883";

    const std::string CLIENT_ID = "smart_home_app";

    MqttMessageSender sender(BROKER, CLIENT_ID);

    SmartLamp        lamp       ("Lamp1",       "LAMP-001",   "Lamp",       "home/lamp1");
    SmartThermostat  thermostat ("Thermostat1", "THERMO-001", "Thermostat", "home/thermostat1");
    SmartPlug        plug       ("Plug1",       "PLUG-001",   "Plug",       "home/plug1");
    SmartHygrometer  hygrometer ("Hygrometer1", "HYGRO-001",  "Hygrometer", "home/hygrometer1");
    SmartRadiator    radiator   ("Radiator1",   "RAD-001",    "Radiator",   "home/radiator1");

    lamp.setSender(&sender);

    thermostat.setSender(&sender);
    ThermostatEmulator thermoEmu(&thermostat, &radiator, 16.0f);

    plug.setSender(&sender);
    PlugEmulator plgEmu(&plug);

    hygrometer.setSender(&sender);
    HygrometerEmulator hygroEmu(&hygrometer, 45.0f);

    radiator.setSender(&sender);
    RadiatorEmulator radEmu(&radiator, 10);

    sender.connect();

    thermoEmu.start();
    plgEmu.start();
    hygroEmu.start();
    radEmu.start();

    sender.subscribe("home/lamp1/set",
        [&lamp](const std::string&, const std::string& payload) {
            lamp.onMessageReceived(payload);
        });

    sender.subscribe("home/thermostat1/set",
        [&thermostat](const std::string&, const std::string& payload) {
            thermostat.onMessageReceived(payload);
        });

    sender.subscribe("home/plug1/set",
        [&plug](const std::string&, const std::string& payload) {
            plug.onMessageReceived(payload);
        });

    sender.subscribe("home/hygrometer1/set",
        [&hygrometer](const std::string&, const std::string& payload) {
            hygrometer.onMessageReceived(payload);
        });

    sender.subscribe("home/radiator1/set",
        [&radiator](const std::string&, const std::string& payload) {
            radiator.onMessageReceived(payload);
        });

    std::cout << "\n--- Публикую начальное состояние ---\n";
    lamp.publishState();
    thermostat.publishState();
    plug.publishState();
    hygrometer.publishState();
    radiator.publishState();

    std::cout << "\nВсе устройства готовы. Примеры команд:\n";
    std::cout << "  Лампу включить:\n";
    std::cout << "    mosquitto_pub -h localhost -t home/lamp1/set -m '{\"command\": \"ON\"}'\n";
    std::cout << "  Яркость 75%:\n";
    std::cout << "    mosquitto_pub -h localhost -t home/lamp1/set -m '{\"command\": \"SET_BRIGHTNESS\", \"value\": 75}'\n";
    std::cout << "  Термостат на 25°C в режим HEAT:\n";
    std::cout << "    mosquitto_pub -h localhost -t home/thermostat1/set -m '{\"command\": \"SET_TEMPERATURE\", \"value\": 25}'\n";
    std::cout << "    mosquitto_pub -h localhost -t home/thermostat1/set -m '{\"command\": \"SET_MODE\", \"mode\": \"HEAT\"}'\n";
    std::cout << "  Розетку включить:\n";
    std::cout << "    mosquitto_pub -h localhost -t home/plug1/set -m '{\"command\": \"ON\"}'\n";
    std::cout << "  Влажность 65%:\n";
    std::cout << "    mosquitto_pub -h localhost -t home/hygrometer1/set -m '{\"command\": \"UPDATE_HUMIDITY\", \"value\": 65}'\n";
    std::cout << "  Радиатор включить + целевая 23°C:\n";
    std::cout << "    mosquitto_pub -h localhost -t home/radiator1/set -m '{\"command\": \"ON\"}'\n";
    std::cout << "    mosquitto_pub -h localhost -t home/radiator1/set -m '{\"command\": \"SET_VALVE_LEVEL\", \"value\": 50}'\n";
    std::cout << "\nCtrl+C для выхода.\n" << std::endl;


    std::this_thread::sleep_for(std::chrono::hours(24));

    hygroEmu.stop();
    thermoEmu.stop();
    plgEmu.stop();
    radEmu.stop();
    sender.disconnect();
    return 0;
}
