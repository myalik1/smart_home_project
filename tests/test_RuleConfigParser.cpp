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
