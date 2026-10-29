#pragma once
#include "IEmulator.h"
#include "SmartRadiator.h"
#include <thread>
#include <atomic>
#include <condition_variable>
#include <mutex>

class RadiatorEmulator : public IEmulator {
public:
    RadiatorEmulator(SmartRadiator* rad, int valveStep = 10);
    ~RadiatorEmulator() override;

    void start() override;
    void stop() override;
    void tick() override;

private:
    SmartRadiator* radiator;
    int stepSize;

    std::atomic<bool> isRunning;
    std::thread emuThread;
    std::condition_variable cv;
    std::mutex cv_m;
};
