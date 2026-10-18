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
#include "include/DeviceManager.h"
#include <memory>

int main() {
    const std::string BROKER = "tcp://localhost:1883";
    const std::string CLIENT_ID = "smart_home_app";

    MqttMessageSender sender(BROKER, CLIENT_ID);
    DeviceManager manager(&sender);

    auto lamp       = std::make_shared<SmartLamp>("Lamp1",             "LAMP-001",   "Lamp",       "home/lamp1");
    auto thermostat = std::make_shared<SmartThermostat>("Thermostat1", "THERMO-001", "Thermostat", "home/thermostat1");
    auto plug       = std::make_shared<SmartPlug>("Plug1",             "PLUG-001",   "Plug",       "home/plug1");
    auto hygrometer = std::make_shared<SmartHygrometer>("Hygrometer1", "HYGRO-001",  "Hygrometer", "home/hygrometer1");
    auto radiator   = std::make_shared<SmartRadiator>("Radiator1",     "RAD-001",    "Radiator",   "home/radiator1");

    manager.registerDevice(lamp);
    manager.registerDevice(thermostat);
    manager.registerDevice(plug);
    manager.registerDevice(hygrometer);
    manager.registerDevice(radiator);

    sender.connect();

    std::cout << "\n--- Публикую начальное состояние ---\n";
    manager.publishAllStates();

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
    std::cout << "    mosquitto_pub -h localhost -t home/radiator1/set -m '{\"command\": \"SET_TARGET_TEMP\", \"value\": 23}'\n";
    std::cout << "\nCtrl+C для выхода.\n" << std::endl;


    std::this_thread::sleep_for(std::chrono::hours(24));

    sender.disconnect();
    return 0;
}