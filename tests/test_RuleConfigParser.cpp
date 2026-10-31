#include <gtest/gtest.h>

#include "../include/RuleConfigParser.h"
#include "../include/RuleEngine.h"
#include "../include/DeviceManager.h"
#include "../include/SmartLamp.h"
#include "MockMqttClient.h"

#include <filesystem>

class RuleConfigParserTest : public ::testing::Test {
protected:
    MockMqttClient mockClient;
    std::unique_ptr<DeviceManager> deviceManager;
    std::unique_ptr<RuleEngine> ruleEngine;
    RuleConfigParser parser;

    void SetUp() override {
        deviceManager = std::make_unique<DeviceManager>(&mockClient);
        ruleEngine = std::make_unique<RuleEngine>(deviceManager.get());

        auto lamp = std::make_shared<SmartLamp>("Lamp1", "L-01", "Lamp", "home/lamp1");
        deviceManager->registerDevice(lamp);
    }

    void TearDown() override {
        ruleEngine->stop();
        mockClient.clear();
    }
};

TEST_F(RuleConfigParserTest, ParsesPointTimerFromJson) {
    const std::string jsonConfig = R"({
        "rules": [
            {
                "id": "morning_lamp",
                "type": "point",
                "time": "08:30:00",
                "repeatDaily": true,
                "enabled": true,
                "action": {
                    "device": "Lamp1",
                    "command": {"command": "ON", "brightness": 100}
                }
            }
        ]
    })";

    EXPECT_TRUE(parser.loadFromString(jsonConfig, *ruleEngine));
    EXPECT_EQ(parser.getLastLoadedCount(), 1u);
    EXPECT_EQ(ruleEngine->getTimerCount(), 1u);

    auto timer = ruleEngine->getTimer("morning_lamp");
    ASSERT_NE(timer, nullptr);
    EXPECT_EQ(timer->getId(), "morning_lamp");
    EXPECT_TRUE(timer->isEnabled());

    RuleAction act = timer->getAction();
    EXPECT_EQ(act.targetDevice, "Lamp1");
    EXPECT_NE(act.commandJson.find("\"command\":\"ON\""), std::string::npos);
}

TEST_F(RuleConfigParserTest, ParsesTimeWindowIntervalTimerFromJson) {
    const std::string jsonConfig = R"([
        {
            "id": "office_hours_climate",
            "type": "time_window",
            "startTime": "09:00:00",
            "endTime": "18:00:00",
            "repeatDaily": true,
            "maintainState": true,
            "maintainIntervalSeconds": 30,
            "startAction": {
                "device": "Lamp1",
                "command": {"command": "ON"}
            },
            "endAction": {
                "device": "Lamp1",
                "command": {"command": "OFF"}
            }
        }
    ])";

    EXPECT_TRUE(parser.loadFromString(jsonConfig, *ruleEngine));
    EXPECT_EQ(parser.getLastLoadedCount(), 1u);
    EXPECT_EQ(ruleEngine->getTimerCount(), 1u);

    auto timer = ruleEngine->getTimer("office_hours_climate");
    ASSERT_NE(timer, nullptr);

    auto intervalTimer = std::dynamic_pointer_cast<IntervalTimer>(timer);
    ASSERT_NE(intervalTimer, nullptr);
    EXPECT_EQ(intervalTimer->getMode(), IntervalTimer::Mode::TIME_OF_DAY_WINDOW);
}

TEST_F(RuleConfigParserTest, ParsesPeriodicTimerFromJson) {
    const std::string jsonConfig = R"({
        "timers": [
            {
                "id": "periodic_check",
                "type": "periodic",
                "intervalMs": 250,
                "maxTicks": 5,
                "action": {
                    "topic": "home/system/ping",
                    "command": {"command": "PING"}
                }
            }
        ]
    })";

    EXPECT_TRUE(parser.loadFromString(jsonConfig, *ruleEngine));
    EXPECT_EQ(parser.getLastLoadedCount(), 1u);
    EXPECT_EQ(ruleEngine->getTimerCount(), 1u);

    auto timer = ruleEngine->getTimer("periodic_check");
    ASSERT_NE(timer, nullptr);
    EXPECT_EQ(timer->getAction().targetTopic, "home/system/ping");
}

TEST_F(RuleConfigParserTest, HandlesMalformedJsonAndMissingFieldsGracefully) {
    // 1. Broken JSON syntax
    EXPECT_FALSE(parser.loadFromString("{broken json", *ruleEngine));
    EXPECT_FALSE(parser.getLastError().empty());

    // 2. Empty string
    EXPECT_FALSE(parser.loadFromString("", *ruleEngine));

    // 3. Invalid root type (e.g. number)
    EXPECT_FALSE(parser.loadFromString("12345", *ruleEngine));

    // 4. Object without rules array
    EXPECT_FALSE(parser.loadFromString(R"({"unrelated": 1})", *ruleEngine));
}

TEST_F(RuleConfigParserTest, ExportToJsonAndFileRoundTrip) {
    auto t0 = std::chrono::system_clock::now();
    ruleEngine->addPointTimer("timer1", t0 + std::chrono::seconds(10), RuleAction("Lamp1", R"({"command":"ON"})"));

    std::string exported = parser.exportToJson(*ruleEngine);
    EXPECT_NE(exported.find("timer1"), std::string::npos);
    EXPECT_NE(exported.find("Lamp1"), std::string::npos);

    std::string tmpFile = "test_rules_temp.json";
    EXPECT_TRUE(parser.saveToFile(tmpFile, *ruleEngine));

    RuleEngine newEngine(deviceManager.get());
    EXPECT_TRUE(parser.loadFromFile(tmpFile, newEngine));
    EXPECT_EQ(newEngine.getTimerCount(), 1u);
    EXPECT_TRUE(newEngine.hasTimer("timer1"));

    std::error_code ec;
    std::filesystem::remove(tmpFile, ec);
}

TEST_F(RuleConfigParserTest, ExportPreservesDynamicTimerValuesWithoutHardcoding) {
    // 1. Time of day point timer
    ruleEngine->addTimeOfDayTimer("tod_timer", 14, 30, 45,
                                  RuleAction("Lamp1", R"({"command":"ON"})"), true);

    // 2. Time window with distinct startAction and endAction
    ruleEngine->addTimeOfDayIntervalTimer("window_timer",
                                          10, 15, 20, 19, 45, 50,
                                          RuleAction("Lamp1", R"({"command":"ON"})"),
                                          RuleAction("Lamp1", R"({"command":"OFF"})"),
                                          true, true, std::chrono::seconds(45));

    // 3. Periodic timer with custom interval and maxTicks
    ruleEngine->addPeriodicTimer("periodic_timer",
                                 std::chrono::milliseconds(350),
                                 RuleAction("Lamp1", R"({"command":"PING"})"),
                                 7);

    std::string exportedJson = parser.exportToJson(*ruleEngine);
    auto root = nlohmann::json::parse(exportedJson);
    ASSERT_TRUE(root.contains("rules"));
    ASSERT_EQ(root["rules"].size(), 3u);

    // Verify round-trip into clean engine
    RuleEngine roundTripEngine(deviceManager.get());
    EXPECT_TRUE(parser.loadFromString(exportedJson, roundTripEngine));
    EXPECT_EQ(roundTripEngine.getTimerCount(), 3u);

    auto windowTimer = std::dynamic_pointer_cast<IntervalTimer>(roundTripEngine.getTimer("window_timer"));
    ASSERT_NE(windowTimer, nullptr);
    EXPECT_EQ(windowTimer->getStartHour(), 10);
    EXPECT_EQ(windowTimer->getStartMin(), 15);
    EXPECT_EQ(windowTimer->getStartSec(), 20);
    EXPECT_EQ(windowTimer->getEndHour(), 19);
    EXPECT_EQ(windowTimer->getEndMin(), 45);
    EXPECT_EQ(windowTimer->getEndSec(), 50);
    EXPECT_EQ(windowTimer->getStartAction().commandJson, R"({"command":"ON"})");
    EXPECT_EQ(windowTimer->getEndAction().commandJson, R"({"command":"OFF"})");
    EXPECT_TRUE(windowTimer->isMaintainState());
    EXPECT_EQ(windowTimer->getMaintainInterval().count(), 45);

    auto periodic = std::dynamic_pointer_cast<IntervalTimer>(roundTripEngine.getTimer("periodic_timer"));
    ASSERT_NE(periodic, nullptr);
    EXPECT_EQ(periodic->getPeriodicInterval().count(), 350);
    EXPECT_EQ(periodic->getMaxTicks(), 7u);
}

TEST_F(RuleConfigParserTest, GracefullyHandlesInvalidTypesInsideRuleObjects) {
    const std::string badTypesJson = R"({
        "rules": [
            {
                "id": "bad_ts_string",
                "type": "point_absolute",
                "timestamp": "this_is_not_a_number",
                "action": {"device": "Lamp1", "command": {"command": "ON"}}
            },
            {
                "id": "bad_start_time_number",
                "type": "time_window",
                "startTime": 12345,
                "endTime": "18:00:00",
                "startAction": {"device": "Lamp1", "command": {"command": "ON"}},
                "endAction": {"device": "Lamp1", "command": {"command": "OFF"}}
            },
            {
                "id": "good_timer",
                "type": "point",
                "time": "12:00:00",
                "action": {"device": "Lamp1", "command": {"command": "ON"}}
            }
        ]
    })";

    EXPECT_NO_THROW({
        bool ok = parser.loadFromString(badTypesJson, *ruleEngine);
        EXPECT_TRUE(ok);
    });

    // Only good_timer should be loaded, bad ones safely skipped
    EXPECT_EQ(parser.getLastLoadedCount(), 1u);
    EXPECT_TRUE(ruleEngine->hasTimer("good_timer"));
    EXPECT_FALSE(ruleEngine->hasTimer("bad_ts_string"));
    EXPECT_FALSE(ruleEngine->hasTimer("bad_start_time_number"));
}
