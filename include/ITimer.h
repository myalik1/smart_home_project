#pragma once

#include "RuleAction.h"
#include <string>
#include <chrono>

class ITimer {
public:
    virtual ~ITimer() = default;

    virtual std::string getId() const = 0;
    virtual bool isExecuted() const = 0;
    virtual bool isEnabled() const = 0;
    virtual void setEnabled(bool enabled) = 0;

    // Checks current time and triggers action if criteria are met
    virtual void tick(const std::chrono::system_clock::time_point& now) = 0;

    virtual void reset() = 0;
    virtual RuleAction getAction() const = 0;
};
