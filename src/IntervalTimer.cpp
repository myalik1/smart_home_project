#include "../include/IntervalTimer.h"
#include <ctime>

IntervalTimer::IntervalTimer(std::string id,
                             std::chrono::system_clock::time_point start,
                             std::chrono::system_clock::time_point end,
                             RuleAction startAction,
                             RuleAction endAction,
                             bool maintainState,
                             std::chrono::seconds maintainInterval,
                             TriggerCallback callback)
    : id_(std::move(id)),
      mode_(Mode::TIME_WINDOW),
      callback_(std::move(callback)),
      startAction_(std::move(startAction)),
      endAction_(std::move(endAction)),
      windowStart_(start),
      windowEnd_(end),
      maintainState_(maintainState),
      maintainInterval_(maintainInterval),
      insideWindow_(false),
      executed_(false),
      enabled_(true) {}

IntervalTimer::IntervalTimer(std::string id,
                             int startHour, int startMin, int startSec,
                             int endHour, int endMin, int endSec,
                             RuleAction startAction,
                             RuleAction endAction,
                             bool repeatDaily,
                             bool maintainState,
                             std::chrono::seconds maintainInterval,
                             TriggerCallback callback)
    : id_(std::move(id)),
      mode_(Mode::TIME_OF_DAY_WINDOW),
      callback_(std::move(callback)),
      startAction_(std::move(startAction)),
      endAction_(std::move(endAction)),
      startHour_(startHour), startMin_(startMin), startSec_(startSec),
      endHour_(endHour), endMin_(endMin), endSec_(endSec),
      repeatDaily_(repeatDaily),
      maintainState_(maintainState),
      maintainInterval_(maintainInterval),
      insideWindow_(false),
      executed_(false),
      enabled_(true) {}

IntervalTimer::IntervalTimer(std::string id,
                             std::chrono::milliseconds periodicInterval,
                             RuleAction periodicAction,
                             size_t maxTicks,
                             TriggerCallback callback)
    : id_(std::move(id)),
      mode_(Mode::PERIODIC),
      callback_(std::move(callback)),
      startAction_(std::move(periodicAction)),
      periodicInterval_(periodicInterval),
      maxTicks_(maxTicks),
      currentTicks_(0),
      insideWindow_(false),
      executed_(false),
      enabled_(true) {}

std::string IntervalTimer::getId() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return id_;
}

bool IntervalTimer::isExecuted() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return executed_;
}

bool IntervalTimer::isEnabled() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return enabled_;
}

void IntervalTimer::setEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    enabled_ = enabled;
}

IntervalTimer::Mode IntervalTimer::getMode() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return mode_;
}

bool IntervalTimer::isInsideWindow() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return insideWindow_;
}

size_t IntervalTimer::getTickCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return currentTicks_;
}

void IntervalTimer::setCallback(TriggerCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    callback_ = std::move(callback);
}

RuleAction IntervalTimer::getAction() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return startAction_;
}

RuleAction IntervalTimer::getStartAction() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return startAction_;
}

RuleAction IntervalTimer::getEndAction() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return endAction_;
}

std::chrono::milliseconds IntervalTimer::getPeriodicInterval() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return periodicInterval_;
}

size_t IntervalTimer::getMaxTicks() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return maxTicks_;
}

int IntervalTimer::getStartHour() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return startHour_;
}

int IntervalTimer::getStartMin() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return startMin_;
}

int IntervalTimer::getStartSec() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return startSec_;
}

int IntervalTimer::getEndHour() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return endHour_;
}

int IntervalTimer::getEndMin() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return endMin_;
}

int IntervalTimer::getEndSec() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return endSec_;
}

bool IntervalTimer::isRepeatDaily() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return repeatDaily_;
}

bool IntervalTimer::isMaintainState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return maintainState_;
}

std::chrono::seconds IntervalTimer::getMaintainInterval() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return maintainInterval_;
}

std::chrono::system_clock::time_point IntervalTimer::getWindowStart() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return windowStart_;
}

std::chrono::system_clock::time_point IntervalTimer::getWindowEnd() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return windowEnd_;
}

void IntervalTimer::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    insideWindow_ = false;
    executed_ = false;
    currentTicks_ = 0;
    lastFiredTime_ = std::chrono::system_clock::time_point{};
    lastMaintainTime_ = std::chrono::system_clock::time_point{};
}

void IntervalTimer::tick(const std::chrono::system_clock::time_point& now) {
    TriggerCallback cbToCall = nullptr;
    RuleAction actionToFire;
    std::string timerId;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_) {
            return;
        }

        if (mode_ == Mode::PERIODIC) {
            if (executed_) {
                return;
            }

            if (lastFiredTime_ == std::chrono::system_clock::time_point{}) {
                lastFiredTime_ = now;
            }

            if ((now - lastFiredTime_) >= periodicInterval_) {
                cbToCall = callback_;
                actionToFire = startAction_;
                timerId = id_;

                currentTicks_++;
                lastFiredTime_ = now;

                if (maxTicks_ > 0 && currentTicks_ >= maxTicks_) {
                    executed_ = true;
                }
            }
        }
        else if (mode_ == Mode::TIME_WINDOW) {
            bool inWindow = (now >= windowStart_ && now < windowEnd_);

            if (!insideWindow_ && inWindow) {
                // Entering the window
                insideWindow_ = true;
                lastMaintainTime_ = now;
                cbToCall = callback_;
                actionToFire = startAction_;
                timerId = id_;
            }
            else if (insideWindow_ && !inWindow) {
                // Leaving the window
                insideWindow_ = false;
                executed_ = true;
                cbToCall = callback_;
                actionToFire = endAction_;
                timerId = id_;
            }
            else if (insideWindow_ && inWindow && maintainState_) {
                // Maintaining state inside window
                if ((now - lastMaintainTime_) >= maintainInterval_) {
                    lastMaintainTime_ = now;
                    cbToCall = callback_;
                    actionToFire = startAction_;
                    timerId = id_;
                }
            }
        }
        else if (mode_ == Mode::TIME_OF_DAY_WINDOW) {
            bool inWindow = isTimeOfDayInsideWindow(startHour_, startMin_, startSec_,
                                                   endHour_, endMin_, endSec_, now);

            if (!insideWindow_ && inWindow) {
                // Entering time-of-day window
                insideWindow_ = true;
                lastMaintainTime_ = now;
                cbToCall = callback_;
                actionToFire = startAction_;
                timerId = id_;
            }
            else if (insideWindow_ && !inWindow) {
                // Leaving time-of-day window
                insideWindow_ = false;
                if (!repeatDaily_) {
                    executed_ = true;
                }
                cbToCall = callback_;
                actionToFire = endAction_;
                timerId = id_;
            }
            else if (insideWindow_ && inWindow && maintainState_) {
                // Maintaining state inside time-of-day window
                if ((now - lastMaintainTime_) >= maintainInterval_) {
                    lastMaintainTime_ = now;
                    cbToCall = callback_;
                    actionToFire = startAction_;
                    timerId = id_;
                }
            }
        }
    }

    if (cbToCall) {
        cbToCall(timerId, actionToFire);
    }
}

bool IntervalTimer::isTimeOfDayInsideWindow(int startH, int startM, int startS,
                                            int endH, int endM, int endS,
                                            const std::chrono::system_clock::time_point& tp) {
    std::time_t timeVal = std::chrono::system_clock::to_time_t(tp);
    std::tm tmVal;
#if defined(_WIN32)
    localtime_s(&tmVal, &timeVal);
#else
    localtime_r(&timeVal, &tmVal);
#endif

    int currentSecs = tmVal.tm_hour * 3600 + tmVal.tm_min * 60 + tmVal.tm_sec;
    int startSecs = startH * 3600 + startM * 60 + startS;
    int endSecs = endH * 3600 + endM * 60 + endS;

    if (startSecs <= endSecs) {
        return (currentSecs >= startSecs && currentSecs < endSecs);
    } else {
        // Overnight interval (e.g. 22:00 to 06:00)
        return (currentSecs >= startSecs || currentSecs < endSecs);
    }
}
