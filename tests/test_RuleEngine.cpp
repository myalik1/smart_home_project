#include <gtest/gtest.h>

#include "../include/RuleEngine.h"
#include "../include/OneShotTimer.h"
#include "../include/DeviceManager.h"
#include "../include/SmartLamp.h"
#include "../include/SmartPlug.h"
#include "MockMqttClient.h"

#include <chrono>

class RuleEngineTest : public ::testing::Test {
protected:
    MockMqttClient mockClient;
    std::unique_ptr<DeviceManager> deviceManager;
    std::unique_ptr<RuleEngine> ruleEngine;

    std::shared_ptr<SmartLamp> lamp;
    std::shared_ptr<SmartPlug> plug;

    void SetUp() override {
        deviceManager = std::make_unique<DeviceManager>(&mockClient);
        ruleEngine = std::make_unique<RuleEngine>(deviceManager.get());

        lamp = std::make_shared<SmartLamp>("Lamp1", "L-01", "Lamp", "home/lamp1");
        plug = std::make_shared<SmartPlug>("Plug1", "P-01", "Plug", "home/plug1");

        deviceManager->registerDevice(lamp);
        deviceManager->registerDevice(plug);
    }

    void TearDown() override {
        ruleEngine->stop();
        mockClient.clear();
    }
};

TEST_F(RuleEngineTest, PointTimerTriggersAtExactTargetTime) {
    auto t0 = std::chrono::system_clock::now();
    auto targetTime = t0 + std::chrono::seconds(10);

    RuleAction action("Lamp1", R"({"command": "ON"})");
    auto timer = ruleEngine->addPointTimer("timer_lamp_on", targetTime, action);

    ASSERT_NE(timer, nullptr);
    EXPECT_EQ(timer->getId(), "timer_lamp_on");
    EXPECT_FALSE(timer->isExecuted());

    // 1. Tick before trigger time -> should NOT trigger
    ruleEngine->tick(t0 + std::chrono::seconds(5));
    EXPECT_FALSE(timer->isExecuted());
    EXPECT_EQ(ruleEngine->getExecutionCount(), 0u);
    EXPECT_TRUE(mockClient.lastTopic.empty());

    // 2. Tick at trigger time -> should trigger and route command to MQTT
    ruleEngine->tick(targetTime);
    EXPECT_TRUE(timer->isExecuted());
    EXPECT_EQ(ruleEngine->getExecutionCount(), 1u);
    EXPECT_EQ(mockClient.lastTopic, "home/lamp1/set");
    EXPECT_EQ(mockClient.lastMessage, R"({"command": "ON"})");

    // 3. Tick after trigger time -> should NOT fire again (one-shot)
    mockClient.lastMessage.clear();
    ruleEngine->tick(t0 + std::chrono::seconds(15));
    EXPECT_EQ(ruleEngine->getExecutionCount(), 1u);
    EXPECT_TRUE(mockClient.lastMessage.empty());
}

TEST_F(RuleEngineTest, DisabledPointTimerDoesNotTrigger) {
    auto t0 = std::chrono::system_clock::now();
    auto targetTime = t0 + std::chrono::seconds(10);

    RuleAction action("Plug1", R"({"command": "OFF"})");
    auto timer = ruleEngine->addPointTimer("timer_plug_off", targetTime, action);

    timer->setEnabled(false);
    EXPECT_FALSE(timer->isEnabled());

    ruleEngine->tick(targetTime + std::chrono::seconds(1));
    EXPECT_FALSE(timer->isExecuted());
    EXPECT_EQ(ruleEngine->getExecutionCount(), 0u);

    // Re-enable and reset
    timer->setEnabled(true);
    ruleEngine->tick(targetTime + std::chrono::seconds(1));
    EXPECT_TRUE(timer->isExecuted());
    EXPECT_EQ(ruleEngine->getExecutionCount(), 1u);
}

TEST_F(RuleEngineTest, ResetTimerAllowsRetriggering) {
    auto t0 = std::chrono::system_clock::now();
    auto targetTime = t0 + std::chrono::seconds(5);

    RuleAction action("Lamp1", R"({"command": "SET_BRIGHTNESS", "value": 50})");
    auto timer = ruleEngine->addPointTimer("timer_brightness", targetTime, action);

    ruleEngine->tick(targetTime);
    EXPECT_TRUE(timer->isExecuted());
    EXPECT_EQ(ruleEngine->getExecutionCount(), 1u);

    // Reset timer
    timer->reset();
    EXPECT_FALSE(timer->isExecuted());

    ruleEngine->tick(targetTime + std::chrono::seconds(1));
    EXPECT_TRUE(timer->isExecuted());
    EXPECT_EQ(ruleEngine->getExecutionCount(), 2u);
}

TEST_F(RuleEngineTest, DispatchesDirectTopicActionWhenDeviceNameEmpty) {
    auto t0 = std::chrono::system_clock::now();
    RuleAction action("", R"({"command": "RESET_ALL"})", "home/broadcast/set");

    ruleEngine->addPointTimer("timer_broadcast", t0, action);

    ruleEngine->tick(t0);

    EXPECT_EQ(mockClient.lastTopic, "home/broadcast/set");
    EXPECT_EQ(mockClient.lastMessage, R"({"command": "RESET_ALL"})");
}

TEST_F(RuleEngineTest, ManagesTimersCollectionCorrectly) {
    auto t0 = std::chrono::system_clock::now();

    EXPECT_EQ(ruleEngine->getTimerCount(), 0u);

    ruleEngine->addPointTimer("t1", t0 + std::chrono::seconds(1), RuleAction("Lamp1", "{}"));
    ruleEngine->addPointTimer("t2", t0 + std::chrono::seconds(2), RuleAction("Plug1", "{}"));

    EXPECT_EQ(ruleEngine->getTimerCount(), 2u);
    EXPECT_TRUE(ruleEngine->hasTimer("t1"));
    EXPECT_TRUE(ruleEngine->hasTimer("t2"));
    EXPECT_FALSE(ruleEngine->hasTimer("non_existent"));

    EXPECT_TRUE(ruleEngine->removeTimer("t1"));
    EXPECT_EQ(ruleEngine->getTimerCount(), 1u);
    EXPECT_FALSE(ruleEngine->hasTimer("t1"));

    ruleEngine->clearTimers();
    EXPECT_EQ(ruleEngine->getTimerCount(), 0u);
}

TEST_F(RuleEngineTest, TimeOfDayTimerCalculatesFutureOccurrence) {
    auto now = std::chrono::system_clock::now();
    // Schedule point timer for 23:59:59
    auto timer = ruleEngine->addTimeOfDayTimer("night_timer", 23, 59, 59, RuleAction("Lamp1", R"({"command": "OFF"})"));

    ASSERT_NE(timer, nullptr);
    EXPECT_GT(timer->getTriggerTime(), now);

    // Advance clock past the trigger time
    ruleEngine->tick(timer->getTriggerTime());
    EXPECT_TRUE(timer->isExecuted());
    EXPECT_EQ(ruleEngine->getExecutionCount(), 1u);
}

TEST_F(RuleEngineTest, ActionExecutedListenerCallback) {
    auto t0 = std::chrono::system_clock::now();
    RuleAction action("Lamp1", R"({"command": "ON"})");

    std::string executedId;
    std::string executedDevice;
    ruleEngine->onActionExecuted([&](const std::string& id, const RuleAction& act) {
        executedId = id;
        executedDevice = act.targetDevice;
    });

    ruleEngine->addPointTimer("t_callback", t0, action);
    ruleEngine->tick(t0);

    EXPECT_EQ(executedId, "t_callback");
    EXPECT_EQ(executedDevice, "Lamp1");
}

TEST_F(RuleEngineTest, BackgroundThreadStartsAndStopsGracefully) {
    EXPECT_FALSE(ruleEngine->isRunning());
    ruleEngine->start(std::chrono::milliseconds(50));
    EXPECT_TRUE(ruleEngine->isRunning());

    // Calling start again when already running does nothing
    ruleEngine->start(std::chrono::milliseconds(50));
    EXPECT_TRUE(ruleEngine->isRunning());

    ruleEngine->stop();
    EXPECT_FALSE(ruleEngine->isRunning());
}


TEST_F(RuleEngineTest, IntervalTimeWindowTriggersStartOnEntryAndEndOnExit) {
    auto t0 = std::chrono::system_clock::now();
    auto startWindow = t0 + std::chrono::seconds(10);
    auto endWindow = t0 + std::chrono::seconds(30);

    RuleAction startAct("Plug1", R"({"command": "ON"})");
    RuleAction endAct("Plug1", R"({"command": "OFF"})");

    auto timer = ruleEngine->addTimeWindowTimer(
        "window_work_hours", startWindow, endWindow, startAct, endAct);

    ASSERT_NE(timer, nullptr);
    EXPECT_FALSE(timer->isInsideWindow());
    EXPECT_FALSE(timer->isExecuted());

    // 1. Before window -> nothing fired
    ruleEngine->tick(t0 + std::chrono::seconds(5));
    EXPECT_FALSE(timer->isInsideWindow());
    EXPECT_EQ(ruleEngine->getExecutionCount(), 0u);

    // 2. Entering window -> startAction fired
    ruleEngine->tick(startWindow);
    EXPECT_TRUE(timer->isInsideWindow());
    EXPECT_EQ(ruleEngine->getExecutionCount(), 1u);
    EXPECT_EQ(mockClient.lastTopic, "home/plug1/set");
    EXPECT_EQ(mockClient.lastMessage, R"({"command": "ON"})");

    // 3. Inside window without maintainState -> does not duplicate
    mockClient.lastMessage.clear();
    ruleEngine->tick(t0 + std::chrono::seconds(20));
    EXPECT_TRUE(timer->isInsideWindow());
    EXPECT_EQ(ruleEngine->getExecutionCount(), 1u);
    EXPECT_TRUE(mockClient.lastMessage.empty());

    // 4. Leaving window -> endAction fired
    ruleEngine->tick(endWindow);
    EXPECT_FALSE(timer->isInsideWindow());
    EXPECT_TRUE(timer->isExecuted());
    EXPECT_EQ(ruleEngine->getExecutionCount(), 2u);
    EXPECT_EQ(mockClient.lastMessage, R"({"command": "OFF"})");
}

TEST_F(RuleEngineTest, IntervalTimerMaintainsStatePeriodicallyInsideWindow) {
    auto t0 = std::chrono::system_clock::now();
    auto startWindow = t0;
    auto endWindow = t0 + std::chrono::seconds(40);

    RuleAction startAct("Lamp1", R"({"command": "SET_BRIGHTNESS", "value": 75})");
    RuleAction endAct("Lamp1", R"({"command": "OFF"})");

    // Maintain state every 10 seconds inside the window
    auto timer = ruleEngine->addTimeWindowTimer(
        "maintain_lamp", startWindow, endWindow, startAct, endAct,
        true, std::chrono::seconds(10));

    // Entry at t0 -> initial assert (1st fire)
    ruleEngine->tick(t0);
    EXPECT_EQ(ruleEngine->getExecutionCount(), 1u);
    EXPECT_EQ(mockClient.lastMessage, R"({"command": "SET_BRIGHTNESS", "value": 75})");

    // At t0 + 5s -> interval not reached yet
    ruleEngine->tick(t0 + std::chrono::seconds(5));
    EXPECT_EQ(ruleEngine->getExecutionCount(), 1u);

    // At t0 + 10s -> re-asserts state to maintain (2nd fire)
    ruleEngine->tick(t0 + std::chrono::seconds(10));
    EXPECT_EQ(ruleEngine->getExecutionCount(), 2u);

    // At t0 + 20s -> re-asserts state to maintain (3rd fire)
    ruleEngine->tick(t0 + std::chrono::seconds(20));
    EXPECT_EQ(ruleEngine->getExecutionCount(), 3u);

    // At t0 + 40s -> exit window triggers endAction (4th fire)
    ruleEngine->tick(endWindow);
    EXPECT_EQ(ruleEngine->getExecutionCount(), 4u);
    EXPECT_EQ(mockClient.lastMessage, R"({"command": "OFF"})");
}

TEST_F(RuleEngineTest, PeriodicTimerTriggersEveryIntervalUpToMaxTicks) {
    auto t0 = std::chrono::system_clock::now();
    RuleAction pingAction("Plug1", R"({"command": "PING"})");

    auto periodicTimer = ruleEngine->addPeriodicTimer(
        "periodic_ping", std::chrono::milliseconds(100), pingAction, 3);

    ASSERT_NE(periodicTimer, nullptr);
    EXPECT_EQ(periodicTimer->getMode(), IntervalTimer::Mode::PERIODIC);

    // 1st tick at t0 initializes baseline
    ruleEngine->tick(t0);
    EXPECT_EQ(ruleEngine->getExecutionCount(), 0u);

    // 2nd tick at +100ms fires tick 1
    ruleEngine->tick(t0 + std::chrono::milliseconds(100));
    EXPECT_EQ(ruleEngine->getExecutionCount(), 1u);
    EXPECT_EQ(periodicTimer->getTickCount(), 1u);
    EXPECT_FALSE(periodicTimer->isExecuted());

    // 3rd tick at +200ms fires tick 2
    ruleEngine->tick(t0 + std::chrono::milliseconds(200));
    EXPECT_EQ(ruleEngine->getExecutionCount(), 2u);
    EXPECT_EQ(periodicTimer->getTickCount(), 2u);

    // 4th tick at +300ms fires tick 3 and completes maxTicks
    ruleEngine->tick(t0 + std::chrono::milliseconds(300));
    EXPECT_EQ(ruleEngine->getExecutionCount(), 3u);
    EXPECT_EQ(periodicTimer->getTickCount(), 3u);
    EXPECT_TRUE(periodicTimer->isExecuted());

    // 5th tick at +400ms -> should not fire again
    ruleEngine->tick(t0 + std::chrono::milliseconds(400));
    EXPECT_EQ(ruleEngine->getExecutionCount(), 3u);
}

TEST_F(RuleEngineTest, TimeOfDayWindowCalculationStandardAndOvernight) {
    // Helper lambda to construct a specific hour:min:sec time_point for testing
    auto makeTime = [](int hour, int min, int sec) {
        std::time_t nowTime = std::time(nullptr);
        std::tm tmVal;
#if defined(_WIN32)
        localtime_s(&tmVal, &nowTime);
#else
        localtime_r(&nowTime, &tmVal);
#endif
        tmVal.tm_hour = hour;
        tmVal.tm_min = min;
        tmVal.tm_sec = sec;
        return std::chrono::system_clock::from_time_t(std::mktime(&tmVal));
    };

    // 1. Daytime window: 09:00:00 to 18:00:00
    EXPECT_FALSE(IntervalTimer::isTimeOfDayInsideWindow(9, 0, 0, 18, 0, 0, makeTime(8, 59, 59)));
    EXPECT_TRUE(IntervalTimer::isTimeOfDayInsideWindow(9, 0, 0, 18, 0, 0, makeTime(9, 0, 0)));
    EXPECT_TRUE(IntervalTimer::isTimeOfDayInsideWindow(9, 0, 0, 18, 0, 0, makeTime(12, 30, 0)));
    EXPECT_TRUE(IntervalTimer::isTimeOfDayInsideWindow(9, 0, 0, 18, 0, 0, makeTime(17, 59, 59)));
    EXPECT_FALSE(IntervalTimer::isTimeOfDayInsideWindow(9, 0, 0, 18, 0, 0, makeTime(18, 0, 0)));

    // 2. Overnight window: 22:00:00 to 06:00:00
    EXPECT_FALSE(IntervalTimer::isTimeOfDayInsideWindow(22, 0, 0, 6, 0, 0, makeTime(21, 59, 59)));
    EXPECT_TRUE(IntervalTimer::isTimeOfDayInsideWindow(22, 0, 0, 6, 0, 0, makeTime(22, 0, 0)));
    EXPECT_TRUE(IntervalTimer::isTimeOfDayInsideWindow(22, 0, 0, 6, 0, 0, makeTime(23, 30, 0)));
    EXPECT_TRUE(IntervalTimer::isTimeOfDayInsideWindow(22, 0, 0, 6, 0, 0, makeTime(2, 0, 0)));
    EXPECT_TRUE(IntervalTimer::isTimeOfDayInsideWindow(22, 0, 0, 6, 0, 0, makeTime(5, 59, 59)));
    EXPECT_FALSE(IntervalTimer::isTimeOfDayInsideWindow(22, 0, 0, 6, 0, 0, makeTime(6, 0, 0)));
}
