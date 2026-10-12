#include <gtest/gtest.h>
#include "../include/SmartRadiator.h"
#include "../include/json.hpp"
#include "MockMessageSender.h"

using json = nlohmann::json;

class SmartRadiatorTest : public ::testing::Test {
protected:
    SmartRadiator* radiator;
    MockMessageSender mockSender;

    void SetUp() override {
        radiator = new SmartRadiator("Rad1", "R-01", "Radiator", "home/rad1");
        radiator->setSender(&mockSender);
    }

    void TearDown() override {
        delete radiator;
    }
};

TEST_F(SmartRadiatorTest, ValveOperationsLinkedToPowerState) {
    radiator->onMessageReceived(R"({"command": "SET_VALVE_LEVEL", "value": 50})");
    EXPECT_TRUE(mockSender.lastMessage.empty());

    radiator->onMessageReceived(R"({"command": "ON"})");
    radiator->onMessageReceived(R"({"command": "SET_VALVE_LEVEL", "value": 75})");
    json onResult = json::parse(mockSender.lastMessage);
    EXPECT_EQ(onResult["valveLevel"], 75);

    radiator->onMessageReceived(R"({"command": "OFF"})");
    json offResult = json::parse(mockSender.lastMessage);
    EXPECT_EQ(offResult["valveLevel"], 0);
}

TEST_F(SmartRadiatorTest, UpdatesTemperatures) {
    radiator->onMessageReceived(R"({"command": "SET_TARGET_TEMP", "value": 26.0})");
    json tResult = json::parse(mockSender.lastMessage);
    EXPECT_EQ(tResult["targetTemperature"], 26.0f);

    radiator->onMessageReceived(R"({"command": "UPDATE_CURRENT_TEMP", "value": 18.5})");
    json cResult = json::parse(mockSender.lastMessage);
    EXPECT_EQ(cResult["currentTemperature"], 18.5f);
}

TEST_F(SmartRadiatorTest, IgnoresInvalidJson) {
    radiator->onMessageReceived("invalid_json");
}
