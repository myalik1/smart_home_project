#include <gtest/gtest.h>
#include "../include/SmartPlug.h"
#include "../include/json.hpp"
#include "MockMessageSender.h"

using json = nlohmann::json;

class SmartPlugTest : public ::testing::Test {
protected:
    SmartPlug* plug;
    MockMessageSender mockSender;

    void SetUp() override {
        plug = new SmartPlug("Plug1", "P-01", "Plug", "home/plug1");
        plug->setSender(&mockSender);
    }

    void TearDown() override {
        delete plug;
    }
};

TEST_F(SmartPlugTest, PowerUpdatesOnlyWhenOn) {
    plug->onMessageReceived(R"({"command": "UPDATE_POWER", "value": 150.0})");
    EXPECT_TRUE(mockSender.lastMessage.empty());

    plug->onMessageReceived(R"({"command": "ON"})");
    plug->onMessageReceived(R"({"command": "UPDATE_POWER", "value": 150.0})");
    json result = json::parse(mockSender.lastMessage);
    EXPECT_EQ(result["currentPower"], 150.0f);
}

TEST_F(SmartPlugTest, PowerResetsOnTurnOff) {
    plug->onMessageReceived(R"({"command": "ON"})");
    plug->onMessageReceived(R"({"command": "UPDATE_POWER", "value": 100.0})");
    plug->onMessageReceived(R"({"command": "OFF"})");

    json result = json::parse(mockSender.lastMessage);
    EXPECT_EQ(result["isOn"], false);
    EXPECT_EQ(result["currentPower"], 0.0f);
}

TEST_F(SmartPlugTest, IgnoresInvalidJson) {
    plug->onMessageReceived("invalid_json");
    EXPECT_TRUE(mockSender.lastMessage.empty());
}
