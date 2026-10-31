#pragma once
#include "IEmulator.h"
#include "SmartThermostat.h"
#include <thread>
#include <atomic>
#include <condition_variable>
#include <mutex>

class ThermostatEmulator : public IEmulator {
public:
    ThermostatEmulator(SmartThermostat* thermo, float ambientTemp = 16.0f);
    ~ThermostatEmulator() override;

    void start() override;
    void stop() override;
    void tick() override;

private:
    SmartThermostat* thermostat;
    float roomBaseTemperature;

    std::atomic<bool> isRunning;
    std::thread emuThread;
    std::condition_variable cv;
    std::mutex cv_m;
};
