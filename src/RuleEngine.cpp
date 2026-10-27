#include "../include/RuleEngine.h"
#include <iostream>

RuleEngine::RuleEngine(DeviceManager* deviceManager)
    : deviceManager_(deviceManager), isRunning_(false) {}

RuleEngine::~RuleEngine() {
    stop();
}

void RuleEngine::setDeviceManager(DeviceManager* deviceManager) {
    std::lock_guard<std::mutex> lock(mutex_);
    deviceManager_ = deviceManager;
}

DeviceManager* RuleEngine::getDeviceManager() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return deviceManager_;
}

bool RuleEngine::addTimer(std::shared_ptr<ITimer> timer) {
    if (!timer) {
        return false;
    }

    const std::string id = timer->getId();
    if (id.empty()) {
        return false;
    }

    // Bind callback if it's a OneShotTimer
    auto oneShot = std::dynamic_pointer_cast<OneShotTimer>(timer);
    if (oneShot) {
        oneShot->setCallback([this](const std::string& timerId, const RuleAction& action) {
            this->executeAction(timerId, action);
        });
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (timers_.find(id) != timers_.end()) {
        return false;
    }

    timers_[id] = std::move(timer);
    return true;
}

std::shared_ptr<OneShotTimer> RuleEngine::addPointTimer(
    const std::string& timerId,
    std::chrono::system_clock::time_point triggerTime,
    const RuleAction& action) {

    auto timer = std::make_shared<OneShotTimer>(
        timerId, triggerTime, action,
        [this](const std::string& id, const RuleAction& act) {
            this->executeAction(id, act);
        });

    std::lock_guard<std::mutex> lock(mutex_);
    timers_[timerId] = timer;
    return timer;
}

std::shared_ptr<OneShotTimer> RuleEngine::addTimeOfDayTimer(
    const std::string& timerId,
    int hour, int minute, int second,
    const RuleAction& action,
    bool repeatDaily) {

    auto timer = std::make_shared<OneShotTimer>(
        timerId, hour, minute, second, action, repeatDaily,
        [this](const std::string& id, const RuleAction& act) {
            this->executeAction(id, act);
        });

    std::lock_guard<std::mutex> lock(mutex_);
    timers_[timerId] = timer;
    return timer;
}

bool RuleEngine::removeTimer(const std::string& timerId) {
    std::lock_guard<std::mutex> lock(mutex_);
    return timers_.erase(timerId) > 0;
}

bool RuleEngine::hasTimer(const std::string& timerId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return timers_.find(timerId) != timers_.end();
}

std::shared_ptr<ITimer> RuleEngine::getTimer(const std::string& timerId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = timers_.find(timerId);
    if (it != timers_.end()) {
        return it->second;
    }
    return nullptr;
}

size_t RuleEngine::getTimerCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return timers_.size();
}

std::vector<std::shared_ptr<ITimer>> RuleEngine::getAllTimers() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::shared_ptr<ITimer>> result;
    result.reserve(timers_.size());
    for (const auto& [id, timer] : timers_) {
        result.push_back(timer);
    }
    return result;
}

void RuleEngine::clearTimers() {
    std::lock_guard<std::mutex> lock(mutex_);
    timers_.clear();
}

void RuleEngine::tick(const std::chrono::system_clock::time_point& now) {
    std::vector<std::shared_ptr<ITimer>> timersCopy;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        timersCopy.reserve(timers_.size());
        for (const auto& [id, timer] : timers_) {
            timersCopy.push_back(timer);
        }
    }

    for (const auto& timer : timersCopy) {
        if (timer) {
            timer->tick(now);
        }
    }
}

void RuleEngine::start(std::chrono::milliseconds checkInterval) {
    if (isRunning_) {
        return;
    }

    isRunning_ = true;
    workerThread_ = std::thread([this, checkInterval]() {
        while (isRunning_) {
            this->tick(std::chrono::system_clock::now());

            std::unique_lock<std::mutex> lock(cvMutex_);
            cv_.wait_for(lock, checkInterval, [this]() {
                return !isRunning_.load();
            });
        }
    });
}

void RuleEngine::stop() {
    if (!isRunning_) {
        return;
    }

    isRunning_ = false;
    cv_.notify_all();

    if (workerThread_.joinable()) {
        workerThread_.join();
    }
}

bool RuleEngine::isRunning() const {
    return isRunning_;
}

void RuleEngine::onActionExecuted(ActionExecutedListener listener) {
    std::lock_guard<std::mutex> lock(mutex_);
    actionListeners_.push_back(std::move(listener));
}

std::vector<RuleEngine::ExecutionRecord> RuleEngine::getExecutionHistory() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return executionHistory_;
}

size_t RuleEngine::getExecutionCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return executionHistory_.size();
}

void RuleEngine::clearExecutionHistory() {
    std::lock_guard<std::mutex> lock(mutex_);
    executionHistory_.clear();
}

void RuleEngine::executeAction(const std::string& timerId, const RuleAction& action) {
    DeviceManager* dm = nullptr;
    std::vector<ActionExecutedListener> listenersCopy;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        dm = deviceManager_;
        executionHistory_.push_back({
            timerId,
            action,
            std::chrono::system_clock::now()
        });
        listenersCopy = actionListeners_;
    }

    // 1. Dispatch command to DeviceManager
    if (dm) {
        if (!action.targetDevice.empty()) {
            dm->sendCommand(action.targetDevice, action.commandJson);
        } else if (!action.targetTopic.empty()) {
            dm->sendCommandToTopic(action.targetTopic, action.commandJson);
        }
    }

    // 2. Notify listeners
    for (const auto& listener : listenersCopy) {
        if (listener) {
            listener(timerId, action);
        }
    }
}
