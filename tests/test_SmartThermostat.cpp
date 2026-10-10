#include <gtest/gtest.h>
#include "../include/SmartThermostat.h"
#include "../include/json.hpp"
#include "MockMessageSender.h"

using json = nlohmann::json;

class SmartThermostatTest : public ::testing::Test {
protected:
    SmartThermostat* thermostat;
    MockMessageSender mockSender;

    void SetUp() override {
        thermostat = new SmartThermostat("Therm1", "T-01", "Thermostat", "home/therm1");
        thermostat->setSender(&mockSender);
    }

    void TearDown() override {
        delete thermostat;
    }
};

TEST_F(SmartThermostatTest, ChangesModeAndTemperature) {
    thermostat->onMessageReceived(R"({"command": "SET_MODE", "mode": "HEAT"})");
    json modeResult = json::parse(mockSender.lastMessage);
    EXPECT_EQ(modeResult["mode"], "HEAT");

    thermostat->onMessageReceived(R"({"command": "SET_TEMPERATURE", "value": 25.5})");
    json tempResult = json::parse(mockSender.lastMessage);
    EXPECT_EQ(tempResult["targetTemperature"], 25.5f);
}

TEST_F(SmartThermostatTest, ClampsTemperature) {
    thermostat->onMessageReceived(R"({"command": "SET_TEMPERATURE", "value": 50.0})");
    json result = json::parse(mockSender.lastMessage);
    EXPECT_EQ(result["targetTemperature"], 35.0f);
}

TEST_F(SmartThermostatTest, IgnoresInvalidMode) {
    thermostat->onMessageReceived(R"({"command": "SET_MODE", "mode": "FROST"})");
    EXPECT_TRUE(mockSender.lastMessage.empty());
}

TEST_F(SmartThermostatTest, UpdatesCurrentTemperature) {
    thermostat->onMessageReceived(R"({"command": "UPDATE_CURRENT_TEMP", "value": 24.5})");
    json result = json::parse(mockSender.lastMessage);
    EXPECT_EQ(result["currentTemperature"], 24.5f);
}

TEST_F(SmartThermostatTest, IgnoresInvalidJson) {
    thermostat->onMessageReceived("invalid_json");
    EXPECT_TRUE(mockSender.lastMessage.empty());
}
