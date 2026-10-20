#include "../include/HygrometerEmulator.h"
#include <chrono>
#include <cstdlib>
#include <cmath>

HygrometerEmulator::HygrometerEmulator(SmartHygrometer* hygro, float baseHum)
    : hygrometer(hygro), baseHumidity(baseHum), isRunning(false) {
    hygrometer->setCurrentHumidity(baseHumidity);
}

HygrometerEmulator::~HygrometerEmulator() {
    stop();
}

void HygrometerEmulator::start() {
    if (isRunning) return;
    isRunning = true;

    emuThread = std::thread([this]() {
        while (isRunning) {
            tick();
            std::this_thread::sleep_for(std::chrono::seconds(3));
        }
    });
}

void HygrometerEmulator::stop() {
    isRunning = false;
    if (emuThread.joinable()) {
        emuThread.join();
    }
}

void HygrometerEmulator::tick() {
    float current = hygrometer->getCurrentHumidity();
    float next = current;

    float diff = baseHumidity - current;

    if (std::abs(diff) > 1.5f) {
        float step = (diff > 0) ? 0.5f : -0.5f;
        next += step;
    }
    else {
        int randStep = (std::rand() % 3) - 1;
        next += randStep * 0.2f;

        if (next > baseHumidity + 1.5f) next = baseHumidity + 1.5f;
        if (next < baseHumidity - 1.5f) next = baseHumidity - 1.5f;
    }

    if (std::abs(next - current) >= 0.1f) {
        hygrometer->setCurrentHumidity(next);
        hygrometer->publishState();
    }
}
