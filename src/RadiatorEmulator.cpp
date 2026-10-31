#include "../include/RadiatorEmulator.h"
#include <chrono>
#include <algorithm>

RadiatorEmulator::RadiatorEmulator(SmartRadiator* rad, int valveStep)
    : radiator(rad), stepSize(valveStep), isRunning(false) {}

RadiatorEmulator::~RadiatorEmulator() {
    stop();
}

void RadiatorEmulator::start() {
    if (isRunning) return;
    isRunning = true;

    emuThread = std::thread([this]() {
        while (isRunning) {
            tick();
            std::unique_lock<std::mutex> lock(cv_m);
            cv.wait_for(lock, std::chrono::seconds(1), [this]() { return !isRunning.load(); });
        }
    });
}

void RadiatorEmulator::stop() {
    isRunning = false;
    cv.notify_all();
    if (emuThread.joinable()) {
        emuThread.join();
    }
}

void RadiatorEmulator::tick() {
    int current = radiator->getCurrentValveLevel();
    int target = radiator->getIsOn() ? radiator->getTargetValveLevel() : 0;

    if (current == target) return;

    if (current < target) {
        current = std::min(current + stepSize, target);
    } else {
        current = std::max(current - stepSize, target);
    }

    radiator->setCurrentValveLevel(current);
    radiator->publishState();
}
