#include <gtest/gtest.h>
#include "../include/SmartHygrometer.h"
#include "../include/json.hpp"
#include "MockMessageSender.h"

using json = nlohmann::json;

TEST(SmartHygrometerTest, ClampsHumidity) {
    SmartHygrometer hygrometer("Hygro1", "H-01", "Hygrometer", "home/hygro");
    MockMessageSender mockSender;
    hygrometer.setSender(&mockSender);

    hygrometer.onMessageReceived(R"({"command": "UPDATE_HUMIDITY", "value": 120.0})");
    json resultHigh = json::parse(mockSender.lastMessage);
    EXPECT_EQ(resultHigh["currentHumidity"], 100.0f);

    hygrometer.onMessageReceived(R"({"command": "UPDATE_HUMIDITY", "value": -10.0})");
    json resultLow = json::parse(mockSender.lastMessage);
    EXPECT_EQ(resultLow["currentHumidity"], 0.0f);
}

TEST(SmartHygrometerTest, IgnoresInvalidJson) {
    SmartHygrometer hygrometer("Hygro1", "H-01", "Hygrometer", "home/hygro");
    MockMessageSender mockSender;
    hygrometer.setSender(&mockSender);
    hygrometer.onMessageReceived("invalid_json");
    EXPECT_TRUE(mockSender.lastMessage.empty());
}
