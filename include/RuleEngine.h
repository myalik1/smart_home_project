#pragma once

#include "ITimer.h"
#include "OneShotTimer.h"
#include "IntervalTimer.h"
#include "RuleAction.h"
#include "DeviceManager.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <atomic>

class RuleEngine {
public:
    using ActionExecutedListener = std::function<void(const std::string& timerId, const RuleAction& action)>;

    struct ExecutionRecord {
        std::string timerId;
        RuleAction action;
        std::chrono::system_clock::time_point executedAt;
    };

    explicit RuleEngine(DeviceManager* deviceManager = nullptr);
    virtual ~RuleEngine();

    void setDeviceManager(DeviceManager* deviceManager);
    DeviceManager* getDeviceManager() const;

    // Timer management
    bool addTimer(std::shared_ptr<ITimer> timer);

    // Convenience method to add point-in-time timer (PPT-17)
    std::shared_ptr<OneShotTimer> addPointTimer(
        const std::string& timerId,
        std::chrono::system_clock::time_point triggerTime,
        const RuleAction& action);

    // Convenience method to add daily / time-of-day point timer (HH:MM:SS) (PPT-17)
    std::shared_ptr<OneShotTimer> addTimeOfDayTimer(
        const std::string& timerId,
        int hour, int minute, int second,
        const RuleAction& action,
        bool repeatDaily = false);

    // Convenience methods for interval timers (PPT-18)
    std::shared_ptr<IntervalTimer> addTimeWindowTimer(
        const std::string& timerId,
        const std::chrono::system_clock::time_point& start,
        const std::chrono::system_clock::time_point& end,
        const RuleAction& startAction,
        const RuleAction& endAction,
        bool maintainState = false,
        std::chrono::seconds maintainInterval = std::chrono::seconds(60));

    std::shared_ptr<IntervalTimer> addTimeOfDayIntervalTimer(
        const std::string& timerId,
        int startHour, int startMin, int startSec,
        int endHour, int endMin, int endSec,
        const RuleAction& startAction,
        const RuleAction& endAction,
        bool repeatDaily = true,
        bool maintainState = false,
        std::chrono::seconds maintainInterval = std::chrono::seconds(60));

    std::shared_ptr<IntervalTimer> addPeriodicTimer(
        const std::string& timerId,
        std::chrono::milliseconds interval,
        const RuleAction& action,
        size_t maxTicks = 0);

    bool removeTimer(const std::string& timerId);
    bool hasTimer(const std::string& timerId) const;
    std::shared_ptr<ITimer> getTimer(const std::string& timerId) const;
    size_t getTimerCount() const;
    std::vector<std::shared_ptr<ITimer>> getAllTimers() const;
    void clearTimers();

    // Deterministic tick method (crucial for unit tests and step-by-step simulations)
    void tick(const std::chrono::system_clock::time_point& now);

    // Background thread runner
    void start(std::chrono::milliseconds checkInterval = std::chrono::milliseconds(500));
    void stop();
    bool isRunning() const;

    void onActionExecuted(ActionExecutedListener listener);
    std::vector<ExecutionRecord> getExecutionHistory() const;
    size_t getExecutionCount() const;
    void clearExecutionHistory();

private:
    DeviceManager* deviceManager_;
    mutable std::mutex mutex_;

    std::unordered_map<std::string, std::shared_ptr<ITimer>> timers_;
    std::vector<ActionExecutedListener> actionListeners_;
    std::vector<ExecutionRecord> executionHistory_;

    // Background thread members
    std::atomic<bool> isRunning_{false};
    std::thread workerThread_;
    std::condition_variable cv_;
    std::mutex cvMutex_;

    void executeAction(const std::string& timerId, const RuleAction& action);
};
