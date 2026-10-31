#include <gtest/gtest.h>

#include "../include/RuleEngine.h"
#include "../include/OneShotTimer.h"
#include "../include/IntervalTimer.h"
#include "../include/DeviceManager.h"
#include "../include/SmartLamp.h"
#include "../include/SmartPlug.h"
#include "../include/SmartRadiator.h"
#include "../include/SmartThermostat.h"
#include "../include/SmartHygrometer.h"
#include "MockMqttClient.h"

#include <chrono>
#include <thread>
#include <vector>
#include <atomic>

class TimerCoverageTest : public ::testing::Test {
protected:
    MockMqttClient mockClient;
    std::unique_ptr<DeviceManager> deviceManager;
    std::unique_ptr<RuleEngine> ruleEngine;

    std::shared_ptr<SmartLamp> lamp;
    std::shared_ptr<SmartPlug> plug;
    std::shared_ptr<SmartRadiator> radiator;
    std::shared_ptr<SmartThermostat> thermostat;
    std::shared_ptr<SmartHygrometer> hygrometer;

    void SetUp() override {
        deviceManager = std::make_unique<DeviceManager>(&mockClient);
        ruleEngine = std::make_unique<RuleEngine>(deviceManager.get());

        lamp = std::make_shared<SmartLamp>("Lamp1", "L-01", "Lamp", "home/lamp1");
        plug = std::make_shared<SmartPlug>("Plug1", "P-01", "Plug", "home/plug1");
        radiator = std::make_shared<SmartRadiator>("Rad1", "R-01", "Radiator", "home/rad1");
        thermostat = std::make_shared<SmartThermostat>("Therm1", "T-01", "Thermostat", "home/therm1");
        hygrometer = std::make_shared<SmartHygrometer>("Hygro1", "H-01", "Hygrometer", "home/hygro1");

        deviceManager->registerDevice(lamp);
        deviceManager->registerDevice(plug);
        deviceManager->registerDevice(radiator);
        deviceManager->registerDevice(thermostat);
        deviceManager->registerDevice(hygrometer);
    }

    void TearDown() override {
        ruleEngine->stop();
        mockClient.clear();
    }
};

TEST_F(TimerCoverageTest, MultipleTimersTriggerSimultaneouslyAtSameTick) {
    auto t0 = std::chrono::system_clock::now();
    auto fireTime = t0 + std::chrono::seconds(5);

    ruleEngine->addPointTimer("t_lamp", fireTime, RuleAction("Lamp1", R"({"command": "ON"})"));
    ruleEngine->addPointTimer("t_plug", fireTime, RuleAction("Plug1", R"({"command": "ON"})"));
    ruleEngine->addPointTimer("t_rad", fireTime, RuleAction("Rad1", R"({"command": "SET_VALVE_LEVEL", "value": 50})"));
    ruleEngine->addPointTimer("t_therm", fireTime, RuleAction("Therm1", R"({"command": "SET_TEMPERATURE", "value": 24.0})"));
    ruleEngine->addPointTimer("t_hygro", fireTime, RuleAction("Hygro1", R"({"command": "UPDATE_HUMIDITY", "value": 60})"));

    EXPECT_EQ(ruleEngine->getTimerCount(), 5u);

    // Before fireTime
    ruleEngine->tick(t0 + std::chrono::seconds(2));
    EXPECT_EQ(ruleEngine->getExecutionCount(), 0u);

    // At fireTime -> all 5 must execute exactly once
    ruleEngine->tick(fireTime);
    EXPECT_EQ(ruleEngine->getExecutionCount(), 5u);

    // After fireTime -> none should re-execute
    ruleEngine->tick(t0 + std::chrono::seconds(10));
    EXPECT_EQ(ruleEngine->getExecutionCount(), 5u);
}

TEST_F(TimerCoverageTest, DailyRepeatingTimerTriggersAcrossConsecutiveDays) {
    auto baseTime = std::chrono::system_clock::now();

    // Create a time-of-day timer with daily recurrence
    auto timer = std::make_shared<OneShotTimer>(
        "daily_evening_light",
        20, 0, 0, // 20:00:00
        RuleAction("Lamp1", R"({"command": "ON"})"),
        true // repeatDaily
    );

    int fireCount = 0;
    ruleEngine->onActionExecuted([&](const std::string&, const RuleAction&) {
        fireCount++;
    });

    ruleEngine->addTimer(timer);

    auto firstTrigger = timer->getTriggerTime();

    // 1. Fire on Day 1
    timer->tick(firstTrigger);
    EXPECT_EQ(fireCount, 1);
    EXPECT_FALSE(timer->isExecuted()); // Daily timer stays un-expired for next day
    auto secondTrigger = timer->getTriggerTime();
    EXPECT_GT(secondTrigger, firstTrigger);

    // 2. Fire on Day 2
    timer->tick(secondTrigger);
    EXPECT_EQ(fireCount, 2);
    auto thirdTrigger = timer->getTriggerTime();
    EXPECT_GT(thirdTrigger, secondTrigger);

    // 3. Fire on Day 3
    timer->tick(thirdTrigger);
    EXPECT_EQ(fireCount, 3);
}

TEST_F(TimerCoverageTest, OvernightIntervalWindowFullLifecycle) {
    // Overnight window: 22:00:00 to 06:00:00
    RuleAction startAct("Rad1", R"({"command": "SET_VALVE_LEVEL", "value": 80})");
    RuleAction endAct("Rad1", R"({"command": "SET_VALVE_LEVEL", "value": 0})");

    auto timer = std::make_shared<IntervalTimer>(
        "night_heating",
        22, 0, 0,
        6, 0, 0,
        startAct, endAct,
        true, // repeatDaily
        false // maintainState
    );

    int actionsFired = 0;
    std::string lastFiredCommand;
    ruleEngine->onActionExecuted([&](const std::string&, const RuleAction& act) {
        actionsFired++;
        lastFiredCommand = act.commandJson;
    });

    ruleEngine->addTimer(timer);

    auto makeTime = [](int h, int m, int s) {
        std::time_t nowTime = std::time(nullptr);
        std::tm tmVal;
#if defined(_WIN32)
        localtime_s(&tmVal, &nowTime);
#else
        localtime_r(&nowTime, &tmVal);
#endif
        tmVal.tm_hour = h;
        tmVal.tm_min = m;
        tmVal.tm_sec = s;
        return std::chrono::system_clock::from_time_t(std::mktime(&tmVal));
    };

    // 1. At 21:00:00 -> outside
    timer->tick(makeTime(21, 0, 0));
    EXPECT_FALSE(timer->isInsideWindow());
    EXPECT_EQ(actionsFired, 0);

    // 2. At 22:00:00 -> enters window (startAction)
    timer->tick(makeTime(22, 0, 0));
    EXPECT_TRUE(timer->isInsideWindow());
    EXPECT_EQ(actionsFired, 1);
    EXPECT_NE(lastFiredCommand.find("80"), std::string::npos);

    // 3. At 02:00:00 (midnight passed) -> stays inside window
    timer->tick(makeTime(2, 0, 0));
    EXPECT_TRUE(timer->isInsideWindow());
    EXPECT_EQ(actionsFired, 1);

    // 4. At 06:00:00 -> exits window (endAction)
    timer->tick(makeTime(6, 0, 0));
    EXPECT_FALSE(timer->isInsideWindow());
    EXPECT_EQ(actionsFired, 2);
    EXPECT_NE(lastFiredCommand.find("0"), std::string::npos);
}

TEST_F(TimerCoverageTest, InfinitePeriodicTimerTicksAndResets) {
    RuleAction pingAction("Plug1", R"({"command": "PING"})");

    // 0 maxTicks = infinite periodic
    auto timer = ruleEngine->addPeriodicTimer(
        "infinite_ping", std::chrono::milliseconds(50), pingAction, 0);

    ASSERT_NE(timer, nullptr);

    auto t0 = std::chrono::system_clock::now();
    timer->tick(t0); // initialize reference point

    // Simulate 10 periodic ticks
    for (int i = 1; i <= 10; ++i) {
        timer->tick(t0 + std::chrono::milliseconds(50 * i));
        EXPECT_EQ(timer->getTickCount(), static_cast<size_t>(i));
        EXPECT_FALSE(timer->isExecuted()); // never expires
    }

    EXPECT_EQ(ruleEngine->getExecutionCount(), 10u);

    // Resetting resets tick counter
    timer->reset();
    EXPECT_EQ(timer->getTickCount(), 0u);
}

TEST_F(TimerCoverageTest, RejectsDuplicateTimerIdAndHandlesInvalidTimerGracefully) {
    auto t0 = std::chrono::system_clock::now();
    EXPECT_TRUE(ruleEngine->addTimer(std::make_shared<OneShotTimer>("unique_id", t0, RuleAction("Lamp1", "{}"))));

    // Duplicate ID must be rejected
    EXPECT_FALSE(ruleEngine->addTimer(std::make_shared<OneShotTimer>("unique_id", t0, RuleAction("Plug1", "{}"))));
    EXPECT_EQ(ruleEngine->addPointTimer("unique_id", t0, RuleAction("Lamp1", "{}")), nullptr);
    EXPECT_EQ(ruleEngine->addTimeOfDayTimer("unique_id", 12, 0, 0, RuleAction("Lamp1", "{}")), nullptr);
    EXPECT_EQ(ruleEngine->addTimeWindowTimer("unique_id", t0, t0 + std::chrono::seconds(10), RuleAction("Lamp1", "{}"), RuleAction("Lamp1", "{}")), nullptr);
    EXPECT_EQ(ruleEngine->addTimeOfDayIntervalTimer("unique_id", 9, 0, 0, 18, 0, 0, RuleAction("Lamp1", "{}"), RuleAction("Lamp1", "{}")), nullptr);
    EXPECT_EQ(ruleEngine->addPeriodicTimer("unique_id", std::chrono::milliseconds(100), RuleAction("Lamp1", "{}")), nullptr);
    EXPECT_EQ(ruleEngine->getTimerCount(), 1u);

    // Null pointer must be rejected
    EXPECT_FALSE(ruleEngine->addTimer(nullptr));

    // Timer with empty ID must be rejected
    EXPECT_FALSE(ruleEngine->addTimer(std::make_shared<OneShotTimer>("", t0, RuleAction("Lamp1", "{}"))));
}

TEST_F(TimerCoverageTest, ConcurrentTimerExecutionAndModificationThreadSafety) {
    // Start background thread ticking every 10ms
    ruleEngine->start(std::chrono::milliseconds(10));

    std::atomic<bool> stopWorkers{false};
    std::vector<std::thread> workers;

    // Launch 4 threads adding and removing timers concurrently
    for (int threadId = 0; threadId < 4; ++threadId) {
        workers.emplace_back([this, threadId, &stopWorkers]() {
            int counter = 0;
            while (!stopWorkers.load()) {
                std::string id = "t_" + std::to_string(threadId) + "_" + std::to_string(counter++);
                auto t = std::chrono::system_clock::now() + std::chrono::milliseconds(20);
                ruleEngine->addPointTimer(id, t, RuleAction("Lamp1", R"({"command": "ON"})"));

                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                ruleEngine->removeTimer(id);
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    stopWorkers = true;

    for (auto& w : workers) {
        if (w.joinable()) {
            w.join();
        }
    }

    ruleEngine->stop();
    EXPECT_FALSE(ruleEngine->isRunning());
}
