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

TEST_F(SmartRadiatorTest, ValveOperationsLinkedToPowerStateAndAcceptsFloat) {
    radiator->onMessageReceived(R"({"command": "SET_VALVE_LEVEL", "value": 75.4})");
    json onResult = json::parse(mockSender.lastMessage);
    EXPECT_EQ(onResult["isOn"], true);
    EXPECT_EQ(onResult["targetValveLevel"], 75);

    radiator->onMessageReceived(R"({"command": "OFF"})");
    json offResult = json::parse(mockSender.lastMessage);
    EXPECT_EQ(offResult["isOn"], false);
    EXPECT_EQ(offResult["targetValveLevel"], 0);
}

TEST_F(SmartRadiatorTest, ClampsValveLevel) {
    radiator->onMessageReceived(R"({"command": "ON"})");
    radiator->onMessageReceived(R"({"command": "SET_VALVE_LEVEL", "value": 150})");
    json onResult = json::parse(mockSender.lastMessage);
    EXPECT_EQ(onResult["targetValveLevel"], 100);

    radiator->onMessageReceived(R"({"command": "SET_VALVE_LEVEL", "value": -10})");
    onResult = json::parse(mockSender.lastMessage);
    EXPECT_EQ(onResult["targetValveLevel"], 0);
    EXPECT_EQ(onResult["isOn"], false);
}

TEST_F(SmartRadiatorTest, IgnoresInvalidJson) {
    radiator->onMessageReceived("invalid_json");
    EXPECT_TRUE(mockSender.lastMessage.empty());
}
