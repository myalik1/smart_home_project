#pragma once

#include "ITimer.h"
#include <functional>
#include <mutex>
#include <chrono>

class IntervalTimer : public ITimer {
public:
    using TriggerCallback = std::function<void(const std::string& timerId, const RuleAction& action)>;

    enum class Mode {
        TIME_WINDOW,           // Absolute time window [startTp, endTp]
        TIME_OF_DAY_WINDOW,    // Daily window (e.g., 09:00:00 - 18:00:00)
        PERIODIC               // Periodic recurring timer (every N milliseconds)
    };

    // 1. Constructor for absolute time window [start, end]
    IntervalTimer(std::string id,
                  std::chrono::system_clock::time_point start,
                  std::chrono::system_clock::time_point end,
                  RuleAction startAction,
                  RuleAction endAction,
                  bool maintainState = false,
                  std::chrono::seconds maintainInterval = std::chrono::seconds(60),
                  TriggerCallback callback = nullptr);

    // 2. Constructor for time-of-day window (e.g. 09:00 to 18:00)
    IntervalTimer(std::string id,
                  int startHour, int startMin, int startSec,
                  int endHour, int endMin, int endSec,
                  RuleAction startAction,
                  RuleAction endAction,
                  bool repeatDaily = true,
                  bool maintainState = false,
                  std::chrono::seconds maintainInterval = std::chrono::seconds(60),
                  TriggerCallback callback = nullptr);

    // 3. Constructor for periodic interval timer (every N ms)
    IntervalTimer(std::string id,
                  std::chrono::milliseconds periodicInterval,
                  RuleAction periodicAction,
                  size_t maxTicks = 0,
                  TriggerCallback callback = nullptr);

    std::string getId() const override;
    bool isExecuted() const override;
    bool isEnabled() const override;
    void setEnabled(bool enabled) override;

    void tick(const std::chrono::system_clock::time_point& now) override;
    void reset() override;
    RuleAction getAction() const override;

    Mode getMode() const;
    bool isInsideWindow() const;
    size_t getTickCount() const;

    void setCallback(TriggerCallback callback);

    // Helper method to check if a time is within a daily [start, end] window
    static bool isTimeOfDayInsideWindow(int startH, int startM, int startS,
                                        int endH, int endM, int endS,
                                        const std::chrono::system_clock::time_point& tp);

private:
    std::string id_;
    Mode mode_;
    TriggerCallback callback_;

    RuleAction startAction_;
    RuleAction endAction_;

    // Time window params
    std::chrono::system_clock::time_point windowStart_;
    std::chrono::system_clock::time_point windowEnd_;

    // Time of day window params
    int startHour_ = 0, startMin_ = 0, startSec_ = 0;
    int endHour_ = 0, endMin_ = 0, endSec_ = 0;
    bool repeatDaily_ = true;

    // Periodic params
    std::chrono::milliseconds periodicInterval_{0};
    size_t maxTicks_ = 0;
    size_t currentTicks_ = 0;

    // Maintenance params
    bool maintainState_ = false;
    std::chrono::seconds maintainInterval_{60};
    std::chrono::system_clock::time_point lastMaintainTime_{};

    bool insideWindow_ = false;
    bool executed_ = false;
    bool enabled_ = true;

    std::chrono::system_clock::time_point lastFiredTime_{};

    mutable std::mutex mutex_;
};
