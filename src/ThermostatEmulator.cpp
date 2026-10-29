#include "../include/ThermostatEmulator.h"
#include <chrono>

ThermostatEmulator::ThermostatEmulator(SmartThermostat* thermo, float ambientTemp)
    : thermostat(thermo), roomBaseTemperature(ambientTemp), isRunning(false) {
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
    float currentTemp = thermostat->getCurrentTemperature();
    int valveLevel = thermostat->getRadiatorValveLevel();

    float heatGain = (valveLevel / 100.0f) * 0.5f;
    float heatLoss = (currentTemp > roomBaseTemperature) ? 0.1f : 0.0f;

    float nextTemp = currentTemp + heatGain - heatLoss;
    if (nextTemp < roomBaseTemperature) {
        nextTemp = roomBaseTemperature;
    }

    thermostat->updateCurrentTemperature(nextTemp);
}
