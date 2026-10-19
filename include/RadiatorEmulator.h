#pragma once
#include "IEmulator.h"
#include "SmartRadiator.h"
#include <thread>
#include <atomic>

class RadiatorEmulator : public IEmulator {
public:
    RadiatorEmulator(SmartRadiator* rad, int valveStep = 10);
    ~RadiatorEmulator() override;

    void start() override;
    void stop() override;

private:
    void tick() override;

    SmartRadiator* radiator;
    int stepSize;

    std::atomic<bool> isRunning;
    std::thread emuThread;
};
