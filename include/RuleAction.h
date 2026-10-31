#pragma once

#include <string>

struct RuleAction {
    std::string targetDevice;
    std::string commandJson;
    std::string targetTopic; // Optional: direct topic routing if targetDevice is empty

    RuleAction() = default;
    RuleAction(std::string device, std::string jsonCmd, std::string topic = "")
        : targetDevice(std::move(device)),
          commandJson(std::move(jsonCmd)),
          targetTopic(std::move(topic)) {}
};
