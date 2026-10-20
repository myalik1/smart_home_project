#pragma once
#include "IEmulator.h"
#include "SmartPlug.h"
#include <thread>
#include <atomic>

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
};
