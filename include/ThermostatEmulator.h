#pragma once
#include "IEmulator.h"
#include "SmartThermostat.h"
#include "SmartRadiator.h"
#include <thread>
#include <atomic>

class ThermostatEmulator : public IEmulator {
public:
    ThermostatEmulator(SmartThermostat* thermo, SmartRadiator* rad, float ambientTemp = 16.0f);
    ~ThermostatEmulator() override;

    void start() override;
    void stop() override;

private:
    void tick() override;

    SmartThermostat* thermostat;
    SmartRadiator* radiator;
    float roomBaseTemperature;

    std::atomic<bool> isRunning;
    std::thread emuThread;
};
