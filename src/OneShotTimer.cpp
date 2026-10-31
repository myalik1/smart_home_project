#include "../include/OneShotTimer.h"
#include <ctime>

OneShotTimer::OneShotTimer(std::string id,
                           std::chrono::system_clock::time_point triggerTime,
                           RuleAction action,
                           TriggerCallback callback)
    : id_(std::move(id)),
      triggerTime_(triggerTime),
      action_(std::move(action)),
      callback_(std::move(callback)),
      isTimeOfDayMode_(false),
      repeatDaily_(false),
      executed_(false),
      enabled_(true) {}

OneShotTimer::OneShotTimer(std::string id,
                           int hour, int minute, int second,
                           RuleAction action,
                           bool repeatDaily,
                           TriggerCallback callback)
    : id_(std::move(id)),
      action_(std::move(action)),
      callback_(std::move(callback)),
      isTimeOfDayMode_(true),
      hour_(hour),
      minute_(minute),
      second_(second),
      repeatDaily_(repeatDaily),
      executed_(false),
      enabled_(true) {
    triggerTime_ = calculateNextTimeOfDay(hour_, minute_, second_, std::chrono::system_clock::now());
}

std::string OneShotTimer::getId() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return id_;
}

bool OneShotTimer::isExecuted() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return executed_;
}

bool OneShotTimer::isEnabled() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return enabled_;
}

void OneShotTimer::setEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    enabled_ = enabled;
}

void OneShotTimer::tick(const std::chrono::system_clock::time_point& now) {
    TriggerCallback cbToCall = nullptr;
    RuleAction actionToFire;
    std::string timerId;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_) {
            return;
        }

        if (executed_ && !repeatDaily_) {
            return;
        }

        if (now >= triggerTime_) {
            executed_ = true;
            cbToCall = callback_;
            actionToFire = action_;
            timerId = id_;

            if (repeatDaily_ && isTimeOfDayMode_) {
                triggerTime_ = calculateNextTimeOfDay(hour_, minute_, second_, now + std::chrono::seconds(1));
                executed_ = false;
            }
        }
    }

    if (cbToCall) {
        cbToCall(timerId, actionToFire);
    }
}

void OneShotTimer::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    executed_ = false;
    if (isTimeOfDayMode_) {
        triggerTime_ = calculateNextTimeOfDay(hour_, minute_, second_, std::chrono::system_clock::now());
    }
}

RuleAction OneShotTimer::getAction() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return action_;
}

std::chrono::system_clock::time_point OneShotTimer::getTriggerTime() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return triggerTime_;
}

bool OneShotTimer::isRepeatDaily() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return repeatDaily_;
}

bool OneShotTimer::isTimeOfDayMode() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return isTimeOfDayMode_;
}

int OneShotTimer::getHour() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return hour_;
}

int OneShotTimer::getMinute() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return minute_;
}

int OneShotTimer::getSecond() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return second_;
}

void OneShotTimer::setCallback(TriggerCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    callback_ = std::move(callback);
}

std::chrono::system_clock::time_point OneShotTimer::calculateNextTimeOfDay(
    int hour, int minute, int second,
    const std::chrono::system_clock::time_point& from) {

    std::time_t fromTime = std::chrono::system_clock::to_time_t(from);
    std::tm tmVal;
#if defined(_WIN32)
    localtime_s(&tmVal, &fromTime);
#else
    localtime_r(&fromTime, &tmVal);
#endif

    tmVal.tm_hour = hour;
    tmVal.tm_min = minute;
    tmVal.tm_sec = second;

    std::time_t targetTime = std::mktime(&tmVal);
    auto targetTp = std::chrono::system_clock::from_time_t(targetTime);

    if (targetTp <= from) {
        // Target is earlier today, schedule for tomorrow (+24h)
        targetTp += std::chrono::hours(24);
    }

    return targetTp;
}
