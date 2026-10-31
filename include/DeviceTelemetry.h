#pragma once

#include <string>
#include <unordered_map>
#include <chrono>

struct DeviceTelemetry {
    std::string deviceName;
    std::string deviceType;
    std::string serialNumber;
    std::chrono::system_clock::time_point timestamp;
    std::string rawJson;

    // Metrics (normalized values)
    bool isOn = false;
    float currentTemperature = 0.0f;
    float targetTemperature = 0.0f;
    float currentHumidity = 0.0f;
    float currentPower = 0.0f;
    int brightness = 0;
    int valveLevel = 0;
    int targetValveLevel = 0;
    int currentValveLevel = 0;
    std::string mode = "OFF";

    // Dynamic key-value map for any additional attributes
    std::unordered_map<std::string, double> numericMetrics;
    std::unordered_map<std::string, std::string> stringMetrics;

    bool hasMetric(const std::string& key) const {
        return numericMetrics.find(key) != numericMetrics.end();
    }

    double getMetric(const std::string& key, double defaultVal = 0.0) const {
        auto it = numericMetrics.find(key);
        return (it != numericMetrics.end()) ? it->second : defaultVal;
    }
};
