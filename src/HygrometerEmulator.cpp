#include "../include/HygrometerEmulator.h"
#include <chrono>
#include <cmath>
#include <random>

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
            std::unique_lock<std::mutex> lock(cv_m);
            cv.wait_for(lock, std::chrono::seconds(3), [this]() { return !isRunning.load(); });
        }
    });
}

void HygrometerEmulator::stop() {
    isRunning = false;
    cv.notify_all();
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
    } else {
        static std::mt19937 gen(std::random_device{}());
        static std::uniform_int_distribution<int> dist(-1, 1);
        next += dist(gen) * 0.2f;

        if (next > baseHumidity + 1.5f) next = baseHumidity + 1.5f;
        if (next < baseHumidity - 1.5f) next = baseHumidity - 1.5f;
    }

    if (std::abs(next - current) >= 0.1f) {
        hygrometer->setCurrentHumidity(next);
        hygrometer->publishState();
    }
}
