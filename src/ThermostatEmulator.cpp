#include "../include/ThermostatEmulator.h"
#include <chrono>

ThermostatEmulator::ThermostatEmulator(SmartThermostat* thermo, SmartRadiator* rad, float ambientTemp)
    : thermostat(thermo), radiator(rad), roomBaseTemperature(ambientTemp), isRunning(false) {
    thermostat->updateCurrentTemperature(roomBaseTemperature);
}

ThermostatEmulator::~ThermostatEmulator() {
    stop();
}

void ThermostatEmulator::start() {
    if (isRunning) return;
    isRunning = true;

    emuThread = std::thread([this]() {
        while (isRunning) {
            tick();
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    });
}

void ThermostatEmulator::stop() {
    isRunning = false;
    if (emuThread.joinable()) {
        emuThread.join();
    }
}

void ThermostatEmulator::tick() {
    float currentTemp = thermostat->getCurrentTemperature();
    int valveLevel = (radiator != nullptr) ? radiator->getCurrentValveLevel() : 0;

    float heatGain = (valveLevel / 100.0f) * 0.5f;

    float heatLoss = (currentTemp > roomBaseTemperature) ? 0.1f : 0.0f;

    float nextTemp = currentTemp + heatGain - heatLoss;
    if (nextTemp < roomBaseTemperature) {
        nextTemp = roomBaseTemperature;
    }

    thermostat->updateCurrentTemperature(nextTemp);
}
