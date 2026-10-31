#pragma once

#include "ITimer.h"
#include <functional>
#include <mutex>

class OneShotTimer : public ITimer {
public:
    using TriggerCallback = std::function<void(const std::string& timerId, const RuleAction& action)>;

    // Constructs a timer to trigger at an absolute time point
    OneShotTimer(std::string id,
                 std::chrono::system_clock::time_point triggerTime,
                 RuleAction action,
                 TriggerCallback callback = nullptr);

    // Constructs a point timer to trigger at a specific time of day (HH:MM:SS)
    OneShotTimer(std::string id,
                 int hour, int minute, int second,
                 RuleAction action,
                 bool repeatDaily = false,
                 TriggerCallback callback = nullptr);

    std::string getId() const override;
    bool isExecuted() const override;
    bool isEnabled() const override;
    void setEnabled(bool enabled) override;

    void tick(const std::chrono::system_clock::time_point& now) override;
    void reset() override;
    RuleAction getAction() const override;

    std::chrono::system_clock::time_point getTriggerTime() const;
    bool isRepeatDaily() const;
    bool isTimeOfDayMode() const;
    int getHour() const;
    int getMinute() const;
    int getSecond() const;

    void setCallback(TriggerCallback callback);

    static std::chrono::system_clock::time_point calculateNextTimeOfDay(
        int hour, int minute, int second,
        const std::chrono::system_clock::time_point& from = std::chrono::system_clock::now());

private:
    std::string id_;
    std::chrono::system_clock::time_point triggerTime_;
    RuleAction action_;
    TriggerCallback callback_;

    bool isTimeOfDayMode_ = false;
    int hour_ = 0;
    int minute_ = 0;
    int second_ = 0;
    bool repeatDaily_ = false;

    bool executed_ = false;
    bool enabled_ = true;

    mutable std::mutex mutex_;
};
