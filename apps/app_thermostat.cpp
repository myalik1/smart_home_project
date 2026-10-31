#include "../include/SmartThermostat.h"
#include "../include/ThermostatEmulator.h"
#include "../include/MqttMessageSender.h"
#include <iostream>

int main() {
    MqttMessageSender sender("tcp://localhost:1883", "app_thermostat_client");

    SmartThermostat thermostat("Therm1", "T-01", "Thermostat", "home/therm1", "home/radiator1/set");
    thermostat.setSender(&sender);

    ThermostatEmulator thermoEmu(&thermostat, 16.0f);
    thermoEmu.start();

    sender.connect();

    sender.subscribe("home/therm1/set", [&thermostat](const std::string&, const std::string& payload) {
        thermostat.onMessageReceived(payload);
    });

    sender.subscribe("home/radiator1", [&thermostat](const std::string&, const std::string& payload) {
        thermostat.onMessageReceived(payload);
    });

    thermostat.publishState();

    std::cout << "[Thermostat Micro-app] is running.\n";
    std::cout << "Press Enter to stop and exit...\n";
    std::cin.get();

    thermoEmu.stop();
    sender.disconnect();
    return 0;
}
