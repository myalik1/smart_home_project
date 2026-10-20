#pragma once
#include "IEmulator.h"
#include "SmartHygrometer.h"
#include <thread>
#include <atomic>

class HygrometerEmulator : public IEmulator {
public:
    HygrometerEmulator(SmartHygrometer* hygro, float baseHum = 45.0f);
    ~HygrometerEmulator() override;

    void start() override;
    void stop() override;
    void tick() override;
private:
    SmartHygrometer* hygrometer;
    float baseHumidity;

    std::atomic<bool> isRunning;
    std::thread emuThread;
};
