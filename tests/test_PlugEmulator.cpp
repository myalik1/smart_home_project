#include <gtest/gtest.h>
#include "../include/PlugEmulator.h"
#include "../include/SmartPlug.h"
#include "MockMessageSender.h"
#include "../include/json.hpp"
#include <chrono>
#include <thread>

using json = nlohmann::json;

TEST(PlugEmulatorTest, SimulatesLoadOnValidOnAndZeroesOnOff) {
    MockMessageSender sender;
    SmartPlug plug("Plug1", "P-01", "Plug", "home/plug1");
    plug.setSender(&sender);
    PlugEmulator emulator(&plug);

    plug.onMessageReceived(R"({"command": "ON"})");
    emulator.tick();
    EXPECT_GE(plug.getCurrentPower(), 1990.0f);
    EXPECT_LE(plug.getCurrentPower(), 2010.0f);

    plug.onMessageReceived(R"({"command": "OFF"})");
    emulator.tick();
    EXPECT_FLOAT_EQ(plug.getCurrentPower(), 0.0f);

    int callsBefore = sender.callCount;
    emulator.tick();
    EXPECT_EQ(sender.callCount, callsBefore);
}

TEST(PlugEmulatorTest, DoesNotActivateOnInvalidCommandsAndTestsThreadLifecycle) {
    MockMessageSender sender;
    SmartPlug plug("Plug1", "P-01", "Plug", "home/plug1");
    plug.setSender(&sender);
    PlugEmulator emulator(&plug);

    plug.onMessageReceived("not_a_json");
    plug.onMessageReceived(R"({"command": " TURN_ON_INVALID "})");
    emulator.tick();
    EXPECT_FLOAT_EQ(plug.getCurrentPower(), 0.0f);
    EXPECT_EQ(sender.callCount, 0);

    plug.onMessageReceived(R"({"command": "ON"})");
    emulator.start();
    emulator.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    emulator.stop();
    EXPECT_GT(plug.getCurrentPower(), 1900.0f);
}
