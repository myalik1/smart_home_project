#include "../include/PlugEmulator.h"
#include <chrono>
#include <cstdlib>

PlugEmulator::PlugEmulator(SmartPlug* plg)
    : plug(plg), isRunning(false) {}

PlugEmulator::~PlugEmulator() {
    stop();
}

void PlugEmulator::start() {
    if (isRunning) return;
    isRunning = true;

    emuThread = std::thread([this]() {
        while (isRunning) {
            tick();
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    });
}

void PlugEmulator::stop() {
    isRunning = false;
    if (emuThread.joinable()) {
        emuThread.join();
    }
}

void PlugEmulator::tick() {
    bool isOn = plug->getIsOn();
    float currentPower = plug->getCurrentPower();
    bool changed = false;

    if (isOn) {
        float simulatedLoad = 2000.0f + ((rand() % 20) - 10);

        if (abs(currentPower - simulatedLoad) > 0.1f) {
            plug->setCurrentPower(simulatedLoad);
            changed = true;
        }
    } else {
        if (currentPower > 0.0f) {
            plug->setCurrentPower(0.0f);
            changed = true;
        }
    }

    if (changed) {
        plug->publishState();
    }
}
