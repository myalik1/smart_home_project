#include "../include/ThermostatEmulator.h"
#include <chrono>
#include <cmath>

ThermostatEmulator::ThermostatEmulator(SmartThermostat* thermo, float ambientTemp)
    : thermostat(thermo)
    , roomBaseTemperature(ambientTemp)
    , exactTemperature(ambientTemp)
    , lastPushedRoundedTemp(ambientTemp)
    , isRunning(false) {
    thermostat->updateCurrentTemperature(roomBaseTemperature);
    lastPushedRoundedTemp = thermostat->getCurrentTemperature();
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
            std::unique_lock<std::mutex> lock(cv_m);
            cv.wait_for(lock, std::chrono::seconds(2), [this]() { return !isRunning.load(); });
        }
    });
}

void ThermostatEmulator::stop() {
    isRunning = false;
    cv.notify_all();
    if (emuThread.joinable()) {
        emuThread.join();
    }
}

void ThermostatEmulator::tick() {
    float deviceTemp = thermostat->getCurrentTemperature();
    if (std::abs(deviceTemp - lastPushedRoundedTemp) >= 0.05f) {
        exactTemperature = deviceTemp;
    }

    int valveLevel = thermostat->getRadiatorValveLevel();

    float heatGain = (valveLevel / 100.0f) * 0.5f;
    float heatLoss = (exactTemperature > roomBaseTemperature) ? 0.1f : 0.0f;

    exactTemperature = exactTemperature + heatGain - heatLoss;
    if (exactTemperature < roomBaseTemperature) {
        exactTemperature = roomBaseTemperature;
    }

    thermostat->updateCurrentTemperature(exactTemperature);
    lastPushedRoundedTemp = thermostat->getCurrentTemperature();
}
