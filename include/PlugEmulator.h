#pragma once
#include "IEmulator.h"
#include "SmartPlug.h"
#include <thread>
#include <atomic>
#include <condition_variable>
#include <mutex>

class PlugEmulator : public IEmulator {
public:
    PlugEmulator(SmartPlug* plg);
    ~PlugEmulator() override;

    void start() override;
    void stop() override;
    void tick() override;
private:
    SmartPlug* plug;
    std::atomic<bool> isRunning;
    std::thread emuThread;
    std::condition_variable cv;
    std::mutex cv_m;
};
