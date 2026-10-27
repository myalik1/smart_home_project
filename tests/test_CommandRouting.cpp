#include <gtest/gtest.h>

#include "../include/RuleEngine.h"
#include "../include/DeviceManager.h"
#include "../include/RuleConfigParser.h"
#include "../include/SmartLamp.h"
#include "../include/SmartPlug.h"
#include "../include/SmartThermostat.h"
#include "../include/SmartRadiator.h"
#include "../include/SmartHygrometer.h"
#include "../include/json.hpp"
#include "MockMqttClient.h"

#include <chrono>
#include <memory>
#include <string>
#include <vector>

using json = nlohmann::json;

class CommandRoutingTest : public ::testing::Test {
protected:
    MockMqttClient mockClient;
    std::unique_ptr<DeviceManager> deviceManager;
    std::unique_ptr<RuleEngine> ruleEngine;

    std::shared_ptr<SmartLamp> lamp;
    std::shared_ptr<SmartPlug> plug;
    std::shared_ptr<SmartThermostat> thermostat;
    std::shared_ptr<SmartRadiator> radiator;
    std::shared_ptr<SmartHygrometer> hygrometer;

    void SetUp() override {
        deviceManager = std::make_unique<DeviceManager>(&mockClient);
        ruleEngine = std::make_unique<RuleEngine>(deviceManager.get());

        lamp = std::make_shared<SmartLamp>("Lamp1", "L-001", "Lamp", "home/lamp1");
        plug = std::make_shared<SmartPlug>("Plug1", "P-001", "Plug", "home/plug1");
        thermostat = std::make_shared<SmartThermostat>("Therm1", "T-001", "Thermostat", "home/therm1");
        radiator = std::make_shared<SmartRadiator>("Rad1", "R-001", "Radiator", "home/rad1");
        hygrometer = std::make_shared<SmartHygrometer>("Hygro1", "H-001", "Hygrometer", "home/hygro1");

        deviceManager->registerDevice(lamp);
        deviceManager->registerDevice(plug);
        deviceManager->registerDevice(thermostat);
        deviceManager->registerDevice(radiator);
        deviceManager->registerDevice(hygrometer);
    }

    void TearDown() override {
        ruleEngine->stop();
        mockClient.clear();
    }
};

// ============================================================================
// 1. DeviceManager Direct Routing for all 5 Device Types (PPT-21)
// ============================================================================

TEST_F(CommandRoutingTest, DeviceManagerRoutesCommandsToAllFiveDeviceTypes) {
    // 1. SmartLamp routing
    EXPECT_TRUE(deviceManager->sendCommand("Lamp1", R"({"command": "ON"})"));
    EXPECT_EQ(mockClient.lastTopic, "home/lamp1/set");
    EXPECT_EQ(mockClient.lastMessage, R"({"command": "ON"})");

    EXPECT_TRUE(deviceManager->sendCommand("Lamp1", R"({"command": "SET_BRIGHTNESS", "value": 75})"));
    EXPECT_EQ(mockClient.lastTopic, "home/lamp1/set");
    EXPECT_EQ(mockClient.lastMessage, R"({"command": "SET_BRIGHTNESS", "value": 75})");

    // 2. SmartThermostat routing
    EXPECT_TRUE(deviceManager->sendCommand("Therm1", R"({"command": "SET_TEMPERATURE", "value": 23.5})"));
    EXPECT_EQ(mockClient.lastTopic, "home/therm1/set");
    EXPECT_EQ(mockClient.lastMessage, R"({"command": "SET_TEMPERATURE", "value": 23.5})");

    EXPECT_TRUE(deviceManager->sendCommand("Therm1", R"({"command": "SET_MODE", "mode": "COOL"})"));
    EXPECT_EQ(mockClient.lastTopic, "home/therm1/set");
    EXPECT_EQ(mockClient.lastMessage, R"({"command": "SET_MODE", "mode": "COOL"})");

    // 3. SmartPlug routing
    EXPECT_TRUE(deviceManager->sendCommand("Plug1", R"({"command": "ON"})"));
    EXPECT_EQ(mockClient.lastTopic, "home/plug1/set");
    EXPECT_EQ(mockClient.lastMessage, R"({"command": "ON"})");

    EXPECT_TRUE(deviceManager->sendCommand("Plug1", R"({"command": "OFF"})"));
    EXPECT_EQ(mockClient.lastTopic, "home/plug1/set");
    EXPECT_EQ(mockClient.lastMessage, R"({"command": "OFF"})");

    // 4. SmartRadiator routing
    EXPECT_TRUE(deviceManager->sendCommand("Rad1", R"({"command": "SET_VALVE_LEVEL", "value": 60})"));
    EXPECT_EQ(mockClient.lastTopic, "home/rad1/set");
    EXPECT_EQ(mockClient.lastMessage, R"({"command": "SET_VALVE_LEVEL", "value": 60})");

    EXPECT_TRUE(deviceManager->sendCommand("Rad1", R"({"command": "SET_TARGET_TEMP", "value": 22.0})"));
    EXPECT_EQ(mockClient.lastTopic, "home/rad1/set");
    EXPECT_EQ(mockClient.lastMessage, R"({"command": "SET_TARGET_TEMP", "value": 22.0})");

    // 5. SmartHygrometer routing
    EXPECT_TRUE(deviceManager->sendCommand("Hygro1", R"({"command": "UPDATE_HUMIDITY", "value": 55.0})"));
    EXPECT_EQ(mockClient.lastTopic, "home/hygro1/set");
    EXPECT_EQ(mockClient.lastMessage, R"({"command": "UPDATE_HUMIDITY", "value": 55.0})");

    // Total sent messages should be exactly 9
    EXPECT_EQ(mockClient.sentMessages.size(), 9u);
}

TEST_F(CommandRoutingTest, DeviceManagerDirectTopicRoutingWithoutRegisteredDevice) {
    // Send command directly to custom / broadcast topics
    EXPECT_TRUE(deviceManager->sendCommandToTopic("home/custom/curtains/set", R"({"action": "OPEN", "level": 100})"));
    EXPECT_EQ(mockClient.lastTopic, "home/custom/curtains/set");
    EXPECT_EQ(mockClient.lastMessage, R"({"action": "OPEN", "level": 100})");

    EXPECT_TRUE(deviceManager->sendCommandToTopic("home/broadcast/alert", R"({"level": "WARNING", "msg": "Freeze alert"})"));
    EXPECT_EQ(mockClient.lastTopic, "home/broadcast/alert");
    EXPECT_EQ(mockClient.lastMessage, R"({"level": "WARNING", "msg": "Freeze alert"})");

    EXPECT_EQ(mockClient.sentMessages.size(), 2u);
}

TEST_F(CommandRoutingTest, DeviceManagerRoutingHandlesNonExistentDevice) {
    // Attempting to route to an unknown device must return false and not publish
    EXPECT_FALSE(deviceManager->sendCommand("NonExistentDevice", R"({"command": "ON"})"));
    EXPECT_TRUE(mockClient.lastTopic.empty());
    EXPECT_TRUE(mockClient.lastMessage.empty());
    EXPECT_EQ(mockClient.sentMessages.size(), 0u);
}

TEST_F(CommandRoutingTest, DeviceManagerRoutingHandlesDetachedMqttClientGracefully) {
    // Detach MQTT client
    deviceManager->setMqttClient(nullptr);

    EXPECT_FALSE(deviceManager->sendCommand("Lamp1", R"({"command": "ON"})"));
    EXPECT_FALSE(deviceManager->sendCommandToTopic("home/broadcast", R"({"command": "PING"})"));
    EXPECT_EQ(mockClient.sentMessages.size(), 0u);
}

// ============================================================================
// 2. RuleEngine to DeviceManager Command Routing (PPT-21)
// ============================================================================

TEST_F(CommandRoutingTest, RuleEnginePointTimerRoutesCommandToDeviceTopic) {
    auto t0 = std::chrono::system_clock::now();
    auto targetTime = t0 + std::chrono::seconds(10);

    RuleAction action("Lamp1", R"({"command": "SET_BRIGHTNESS", "value": 85})");
    auto timer = ruleEngine->addPointTimer("timer_lamp_dim", targetTime, action);
    ASSERT_NE(timer, nullptr);

    // Ticking before trigger time must not route any command
    ruleEngine->tick(t0 + std::chrono::seconds(5));
    EXPECT_FALSE(timer->isExecuted());
    EXPECT_EQ(ruleEngine->getExecutionCount(), 0u);
    EXPECT_EQ(mockClient.sentMessages.size(), 0u);

    // Ticking at target time routes command through DeviceManager to MQTT
    ruleEngine->tick(targetTime);
    EXPECT_TRUE(timer->isExecuted());
    EXPECT_EQ(ruleEngine->getExecutionCount(), 1u);
    ASSERT_EQ(mockClient.sentMessages.size(), 1u);
    EXPECT_EQ(mockClient.sentMessages[0].topic, "home/lamp1/set");
    EXPECT_EQ(mockClient.sentMessages[0].payload, R"({"command": "SET_BRIGHTNESS", "value": 85})");

    // History record verification
    auto history = ruleEngine->getExecutionHistory();
    ASSERT_EQ(history.size(), 1u);
    EXPECT_EQ(history[0].timerId, "timer_lamp_dim");
    EXPECT_EQ(history[0].action.targetDevice, "Lamp1");
    EXPECT_EQ(history[0].action.commandJson, R"({"command": "SET_BRIGHTNESS", "value": 85})");
}

TEST_F(CommandRoutingTest, RuleEngineDirectTopicRoutingWhenDeviceNameIsEmpty) {
    auto t0 = std::chrono::system_clock::now();
    auto targetTime = t0 + std::chrono::seconds(5);

    // RuleAction with empty targetDevice and populated targetTopic
    RuleAction action("", R"({"alert": "ALL_OFF"})", "home/global/broadcast");
    auto timer = ruleEngine->addPointTimer("broadcast_timer", targetTime, action);
    ASSERT_NE(timer, nullptr);

    ruleEngine->tick(targetTime);
    EXPECT_TRUE(timer->isExecuted());
    ASSERT_EQ(mockClient.sentMessages.size(), 1u);
    EXPECT_EQ(mockClient.sentMessages[0].topic, "home/global/broadcast");
    EXPECT_EQ(mockClient.sentMessages[0].payload, R"({"alert": "ALL_OFF"})");
}

TEST_F(CommandRoutingTest, RuleEngineRoutesMultiDeviceMacroScenarioSequentially) {
    // "Leaving Home" macro: turns off lamp, turns off plug, lowers heating, sets radiator valve to 0
    auto t0 = std::chrono::system_clock::now();
    auto triggerTime = t0 + std::chrono::seconds(10);

    ruleEngine->addPointTimer("leave_lamp", triggerTime,
                             RuleAction("Lamp1", R"({"command": "OFF"})"));
    ruleEngine->addPointTimer("leave_plug", triggerTime,
                             RuleAction("Plug1", R"({"command": "OFF"})"));
    ruleEngine->addPointTimer("leave_therm", triggerTime,
                             RuleAction("Therm1", R"({"command": "SET_TEMPERATURE", "value": 16.0})"));
    ruleEngine->addPointTimer("leave_rad", triggerTime,
                             RuleAction("Rad1", R"({"command": "SET_VALVE_LEVEL", "value": 0})"));

    EXPECT_EQ(ruleEngine->getTimerCount(), 4u);

    // Trigger all actions
    ruleEngine->tick(triggerTime);

    EXPECT_EQ(ruleEngine->getExecutionCount(), 4u);
    ASSERT_EQ(mockClient.sentMessages.size(), 4u);

    // Build a map of topic -> payload from sentMessages to verify routing order-independently
    std::unordered_map<std::string, std::string> sentMap;
    for (const auto& msg : mockClient.sentMessages) {
        sentMap[msg.topic] = msg.payload;
    }

    EXPECT_EQ(sentMap["home/lamp1/set"], R"({"command": "OFF"})");
    EXPECT_EQ(sentMap["home/plug1/set"], R"({"command": "OFF"})");
    EXPECT_EQ(sentMap["home/therm1/set"], R"({"command": "SET_TEMPERATURE", "value": 16.0})");
    EXPECT_EQ(sentMap["home/rad1/set"], R"({"command": "SET_VALVE_LEVEL", "value": 0})");
}

TEST_F(CommandRoutingTest, RuleEngineIntervalTimerRoutesStartAndEndActionsToDifferentDevices) {
    auto t0 = std::chrono::system_clock::now();
    auto start = t0 + std::chrono::seconds(10);
    auto end = t0 + std::chrono::seconds(30);

    // On start: turn Plug ON; On end: turn Plug OFF and turn Lamp ON
    RuleAction startAction("Plug1", R"({"command": "ON"})");
    RuleAction endAction("Plug1", R"({"command": "OFF"})");

    auto intervalTimer = ruleEngine->addTimeWindowTimer(
        "window_work_hours", start, end, startAction, endAction, false);

    ASSERT_NE(intervalTimer, nullptr);

    // 1. Before start: no routed commands
    ruleEngine->tick(t0 + std::chrono::seconds(5));
    EXPECT_EQ(mockClient.sentMessages.size(), 0u);

    // 2. Entering window: start action routed to Plug1
    ruleEngine->tick(start);
    ASSERT_EQ(mockClient.sentMessages.size(), 1u);
    EXPECT_EQ(mockClient.sentMessages[0].topic, "home/plug1/set");
    EXPECT_EQ(mockClient.sentMessages[0].payload, R"({"command": "ON"})");

    // 3. Inside window: no new commands
    ruleEngine->tick(t0 + std::chrono::seconds(20));
    EXPECT_EQ(mockClient.sentMessages.size(), 1u);

    // 4. Leaving window: end action routed to Plug1
    ruleEngine->tick(end);
    ASSERT_EQ(mockClient.sentMessages.size(), 2u);
    EXPECT_EQ(mockClient.sentMessages[1].topic, "home/plug1/set");
    EXPECT_EQ(mockClient.sentMessages[1].payload, R"({"command": "OFF"})");
}

TEST_F(CommandRoutingTest, RuleEngineStateMaintenancePeriodicRouting) {
    auto t0 = std::chrono::system_clock::now();
    auto start = t0 + std::chrono::seconds(10);
    auto end = t0 + std::chrono::seconds(70);

    // Maintain thermostat at 22.0 C every 20 seconds during the window
    RuleAction startAction("Therm1", R"({"command": "SET_TEMPERATURE", "value": 22.0})");
    RuleAction endAction("Therm1", R"({"command": "SET_TEMPERATURE", "value": 18.0})");

    ruleEngine->addTimeWindowTimer(
        "therm_maintain", start, end, startAction, endAction,
        /*maintainState=*/true, std::chrono::seconds(20));

    // Start -> 1st message
    ruleEngine->tick(start);
    ASSERT_EQ(mockClient.sentMessages.size(), 1u);
    EXPECT_EQ(mockClient.sentMessages[0].topic, "home/therm1/set");
    EXPECT_EQ(mockClient.sentMessages[0].payload, R"({"command": "SET_TEMPERATURE", "value": 22.0})");

    // +20s -> 2nd message (maintained state routed)
    ruleEngine->tick(start + std::chrono::seconds(20));
    ASSERT_EQ(mockClient.sentMessages.size(), 2u);
    EXPECT_EQ(mockClient.sentMessages[1].topic, "home/therm1/set");
    EXPECT_EQ(mockClient.sentMessages[1].payload, R"({"command": "SET_TEMPERATURE", "value": 22.0})");

    // +40s -> 3rd message (maintained state routed)
    ruleEngine->tick(start + std::chrono::seconds(40));
    ASSERT_EQ(mockClient.sentMessages.size(), 3u);
    EXPECT_EQ(mockClient.sentMessages[2].topic, "home/therm1/set");

    // End (at +60s = end) -> end action routed
    ruleEngine->tick(end);
    ASSERT_EQ(mockClient.sentMessages.size(), 4u);
    EXPECT_EQ(mockClient.sentMessages[3].topic, "home/therm1/set");
    EXPECT_EQ(mockClient.sentMessages[3].payload, R"({"command": "SET_TEMPERATURE", "value": 18.0})");
}

TEST_F(CommandRoutingTest, RuleEnginePeriodicTimerRoutesRecurringCommands) {
    RuleAction action("Rad1", R"({"command": "UPDATE_CURRENT_TEMP", "value": 19.5})");
    auto timer = ruleEngine->addPeriodicTimer(
        "rad_heartbeat", std::chrono::milliseconds(100), action, /*maxTicks=*/3);

    auto t0 = std::chrono::system_clock::now();

    // Baseline tick to initialize timer's lastFiredTime_
    ruleEngine->tick(t0);
    EXPECT_EQ(mockClient.sentMessages.size(), 0u);

    // Tick 1
    ruleEngine->tick(t0 + std::chrono::milliseconds(100));
    EXPECT_EQ(mockClient.sentMessages.size(), 1u);

    // Tick 2
    ruleEngine->tick(t0 + std::chrono::milliseconds(200));
    EXPECT_EQ(mockClient.sentMessages.size(), 2u);

    // Tick 3
    ruleEngine->tick(t0 + std::chrono::milliseconds(300));
    EXPECT_EQ(mockClient.sentMessages.size(), 3u);

    // Tick 4 -> maxTicks reached, no more messages
    ruleEngine->tick(t0 + std::chrono::milliseconds(400));
    EXPECT_EQ(mockClient.sentMessages.size(), 3u);

    for (const auto& msg : mockClient.sentMessages) {
        EXPECT_EQ(msg.topic, "home/rad1/set");
        EXPECT_EQ(msg.payload, R"({"command": "UPDATE_CURRENT_TEMP", "value": 19.5})");
    }
}

TEST_F(CommandRoutingTest, RuleEngineHandlesNonExistentTargetDeviceGracefully) {
    auto t0 = std::chrono::system_clock::now();
    RuleAction action("GhostDevice", R"({"command": "ON"})");
    ruleEngine->addPointTimer("ghost_timer", t0 + std::chrono::seconds(5), action);

    bool listenerCalled = false;
    ruleEngine->onActionExecuted([&listenerCalled](const std::string& timerId, const RuleAction& act) {
        if (timerId == "ghost_timer" && act.targetDevice == "GhostDevice") {
            listenerCalled = true;
        }
    });

    // Should not crash, records history, listener invoked, but no MQTT message sent
    ruleEngine->tick(t0 + std::chrono::seconds(5));
    EXPECT_TRUE(listenerCalled);
    EXPECT_EQ(ruleEngine->getExecutionCount(), 1u);
    EXPECT_EQ(mockClient.sentMessages.size(), 0u);
}

TEST_F(CommandRoutingTest, RuleEngineHandlesNullDeviceManagerGracefully) {
    ruleEngine->setDeviceManager(nullptr);
    EXPECT_EQ(ruleEngine->getDeviceManager(), nullptr);

    auto t0 = std::chrono::system_clock::now();
    RuleAction action("Lamp1", R"({"command": "ON"})");
    ruleEngine->addPointTimer("null_dm_timer", t0 + std::chrono::seconds(5), action);

    bool listenerCalled = false;
    ruleEngine->onActionExecuted([&listenerCalled](const std::string&, const RuleAction&) {
        listenerCalled = true;
    });

    ruleEngine->tick(t0 + std::chrono::seconds(5));
    EXPECT_TRUE(listenerCalled);
    EXPECT_EQ(ruleEngine->getExecutionCount(), 1u);
    EXPECT_EQ(mockClient.sentMessages.size(), 0u);
}

// ============================================================================
// 3. End-to-End Closed Loop Command Routing & State Verification (PPT-21)
// ============================================================================

TEST_F(CommandRoutingTest, EndToEndClosedLoopCommandExecutionForLamp) {
    // 1. Rule Engine fires command to turn on Lamp and set brightness to 90
    auto t0 = std::chrono::system_clock::now();
    ruleEngine->addPointTimer(
        "lamp_on_rule",
        t0 + std::chrono::seconds(2),
        RuleAction("Lamp1", R"({"command": "ON"})")
    );
    ruleEngine->addPointTimer(
        "lamp_bright_rule",
        t0 + std::chrono::seconds(3),
        RuleAction("Lamp1", R"({"command": "SET_BRIGHTNESS", "value": 90})")
    );

    // 2. Trigger first rule -> routes "ON" to "home/lamp1/set"
    ruleEngine->tick(t0 + std::chrono::seconds(2));
    ASSERT_EQ(mockClient.sentMessages.size(), 1u);
    EXPECT_EQ(mockClient.sentMessages[0].topic, "home/lamp1/set");

    // Deliver command to Lamp device
    mockClient.simulateIncomingMessage("home/lamp1/set", mockClient.sentMessages[0].payload);
    // Lamp publishes its new state to "home/lamp1"
    ASSERT_EQ(mockClient.sentMessages.size(), 2u);
    EXPECT_EQ(mockClient.sentMessages[1].topic, "home/lamp1");

    // Deliver published state to DeviceManager
    mockClient.simulateIncomingMessage("home/lamp1", mockClient.sentMessages[1].payload);

    // Verify telemetry updated in DeviceManager
    auto lampTelemetry1 = deviceManager->getTelemetry("Lamp1");
    ASSERT_TRUE(lampTelemetry1.has_value());
    EXPECT_TRUE(lampTelemetry1->isOn);
    EXPECT_EQ(lampTelemetry1->brightness, 100); // Default ON sets brightness to 100

    // 3. Trigger second rule -> routes "SET_BRIGHTNESS 90" to "home/lamp1/set"
    ruleEngine->tick(t0 + std::chrono::seconds(3));
    ASSERT_EQ(mockClient.sentMessages.size(), 3u);
    EXPECT_EQ(mockClient.sentMessages[2].topic, "home/lamp1/set");

    // Deliver brightness command to Lamp
    mockClient.simulateIncomingMessage("home/lamp1/set", mockClient.sentMessages[2].payload);
    ASSERT_EQ(mockClient.sentMessages.size(), 4u);
    EXPECT_EQ(mockClient.sentMessages[3].topic, "home/lamp1");

    // Deliver state to DeviceManager
    mockClient.simulateIncomingMessage("home/lamp1", mockClient.sentMessages[3].payload);

    // Verify final closed-loop state in DeviceManager
    auto lampTelemetry2 = deviceManager->getTelemetry("Lamp1");
    ASSERT_TRUE(lampTelemetry2.has_value());
    EXPECT_TRUE(lampTelemetry2->isOn);
    EXPECT_EQ(lampTelemetry2->brightness, 90);
}

TEST_F(CommandRoutingTest, EndToEndClosedLoopCommandExecutionForThermostatAndRadiator) {
    // Rule: Set Thermostat to HEAT mode and 24.0 C
    auto t0 = std::chrono::system_clock::now();
    ruleEngine->addPointTimer("therm_mode", t0 + std::chrono::seconds(1),
                             RuleAction("Therm1", R"({"command": "SET_MODE", "mode": "HEAT"})"));
    ruleEngine->addPointTimer("therm_temp", t0 + std::chrono::seconds(1),
                             RuleAction("Therm1", R"({"command": "SET_TEMPERATURE", "value": 24.0})"));

    // Rule: Turn Radiator ON and set valve to 80%
    ruleEngine->addPointTimer("rad_on", t0 + std::chrono::seconds(1),
                             RuleAction("Rad1", R"({"command": "ON"})"));
    ruleEngine->addPointTimer("rad_valve", t0 + std::chrono::seconds(1),
                             RuleAction("Rad1", R"({"command": "SET_VALVE_LEVEL", "value": 80})"));

    ruleEngine->tick(t0 + std::chrono::seconds(1));

    // Deliver commands to devices and state updates back to DeviceManager
    size_t processed = 0;
    while (processed < mockClient.sentMessages.size()) {
        const auto msg = mockClient.sentMessages[processed++];
        mockClient.simulateIncomingMessage(msg.topic, msg.payload);
    }

    // Verify Thermostat state in DeviceManager
    auto thermTelemetry = deviceManager->getTelemetry("Therm1");
    ASSERT_TRUE(thermTelemetry.has_value());
    EXPECT_EQ(thermTelemetry->mode, "HEAT");
    EXPECT_FLOAT_EQ(thermTelemetry->targetTemperature, 24.0f);

    // Verify Radiator state in DeviceManager
    auto radTelemetry = deviceManager->getTelemetry("Rad1");
    ASSERT_TRUE(radTelemetry.has_value());
    EXPECT_TRUE(radTelemetry->isOn);
    EXPECT_EQ(radTelemetry->valveLevel, 80);
}

// ============================================================================
// 4. Config-Driven Command Routing (Parser -> RuleEngine -> DM -> MQTT) (PPT-21)
// ============================================================================

TEST_F(CommandRoutingTest, ConfigParserToRuleEngineToDeviceRoutingFullPipeline) {
    std::string configJson = R"({
        "timers": [
            {
                "id": "morning_lamp",
                "type": "time_of_day",
                "time": "07:30:00",
                "repeatDaily": true,
                "action": {
                    "device": "Lamp1",
                    "command": "{\"command\": \"ON\"}"
                }
            },
            {
                "id": "work_plug_window",
                "type": "time_window",
                "startTime": "09:00:00",
                "endTime": "18:00:00",
                "repeatDaily": true,
                "maintainState": false,
                "startAction": {
                    "device": "Plug1",
                    "command": "{\"command\": \"ON\"}"
                },
                "endAction": {
                    "device": "Plug1",
                    "command": "{\"command\": \"OFF\"}"
                }
            }
        ]
    })";

    RuleConfigParser parser;
    bool loaded = parser.loadFromString(configJson, *ruleEngine);
    ASSERT_TRUE(loaded) << "Parser error: " << parser.getLastError();
    EXPECT_EQ(parser.getLastLoadedCount(), 2u);
    EXPECT_EQ(ruleEngine->getTimerCount(), 2u);

    // Construct local test day 2026-10-28
    std::tm timeInfo = {};
    timeInfo.tm_year = 2026 - 1900;
    timeInfo.tm_mon = 9;
    timeInfo.tm_mday = 28;

    timeInfo.tm_hour = 7;
    timeInfo.tm_min = 30;
    timeInfo.tm_sec = 0;
    auto tMorning = std::chrono::system_clock::from_time_t(std::mktime(&timeInfo));

    // Tick at 07:30:00 -> morning_lamp should fire and route to home/lamp1/set
    ruleEngine->tick(tMorning);
    ASSERT_EQ(mockClient.sentMessages.size(), 1u);
    EXPECT_EQ(mockClient.sentMessages[0].topic, "home/lamp1/set");
    EXPECT_EQ(mockClient.sentMessages[0].payload, R"({"command": "ON"})");

    // Tick at 09:00:00 -> work_plug_window start action routes to home/plug1/set
    timeInfo.tm_hour = 9;
    timeInfo.tm_min = 0;
    timeInfo.tm_sec = 0;
    auto tWorkStart = std::chrono::system_clock::from_time_t(std::mktime(&timeInfo));

    ruleEngine->tick(tWorkStart);
    ASSERT_EQ(mockClient.sentMessages.size(), 2u);
    EXPECT_EQ(mockClient.sentMessages[1].topic, "home/plug1/set");
    EXPECT_EQ(mockClient.sentMessages[1].payload, R"({"command": "ON"})");

    // Tick at 18:00:00 -> work_plug_window end action routes to home/plug1/set
    timeInfo.tm_hour = 18;
    timeInfo.tm_min = 0;
    timeInfo.tm_sec = 0;
    auto tWorkEnd = std::chrono::system_clock::from_time_t(std::mktime(&timeInfo));

    ruleEngine->tick(tWorkEnd);
    ASSERT_EQ(mockClient.sentMessages.size(), 3u);
    EXPECT_EQ(mockClient.sentMessages[2].topic, "home/plug1/set");
    EXPECT_EQ(mockClient.sentMessages[2].payload, R"({"command": "OFF"})");
}
