#include "../include/PlugEmulator.h"
#include <chrono>
#include <cmath>
#include <random>

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
            std::unique_lock<std::mutex> lock(cv_m);
            cv.wait_for(lock, std::chrono::seconds(2), [this]{ return !isRunning; });
        }
    });
}

void PlugEmulator::stop() {
    isRunning = false;
    cv.notify_all();
    if (emuThread.joinable()) {
        emuThread.join();
    }
}

void PlugEmulator::tick() {
    bool isOn = plug->getIsOn();
    float currentPower = plug->getCurrentPower();
    bool changed = false;

    if (isOn) {
        static std::mt19937 gen(std::random_device{}());
        static std::uniform_real_distribution<float> dist(-10.0f, 10.0f);
        float simulatedLoad = 2000.0f + dist(gen);

        if (std::abs(currentPower - simulatedLoad) > 0.1f) {
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
