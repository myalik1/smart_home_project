#include <gtest/gtest.h>
#include "../include/SmartLamp.h"
#include "../include/json.hpp"
#include "MockMessageSender.h"

using json = nlohmann::json;

class SmartLampTest : public ::testing::Test {
protected:
    SmartLamp* lamp;
    MockMessageSender mockSender;

    void SetUp() override {
        lamp = new SmartLamp("Lamp1", "L-01", "Lamp", "home/lamp1");
        lamp->setSender(&mockSender);
    }

    void TearDown() override {
        delete lamp;
    }
};

TEST_F(SmartLampTest, TurnsOnAndSendsStatus) {
    lamp->onMessageReceived(R"({"command": "ON"})");

    json result = json::parse(mockSender.lastMessage);
    EXPECT_EQ(result["isOn"], true);
    EXPECT_EQ(result["brightness"], 100);
}

TEST_F(SmartLampTest, ClampsBrightness) {
    lamp->onMessageReceived(R"({"command": "SET_BRIGHTNESS", "value": 150})");
    json resultHigh = json::parse(mockSender.lastMessage);
    EXPECT_EQ(resultHigh["brightness"], 100);

    lamp->onMessageReceived(R"({"command": "SET_BRIGHTNESS", "value": -20})");
    json resultLow = json::parse(mockSender.lastMessage);
    EXPECT_EQ(resultLow["brightness"], 0);
}

TEST_F(SmartLampTest, IgnoresInvalidJson) {
    lamp->onMessageReceived("invalid_json_string");
    EXPECT_TRUE(mockSender.lastMessage.empty());
}
