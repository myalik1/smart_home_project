#pragma once
#include "IEmulator.h"
#include "SmartHygrometer.h"
#include <thread>
#include <atomic>
#include <condition_variable>
#include <mutex>

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
    std::condition_variable cv;
    std::mutex cv_m;
};
