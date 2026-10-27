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
