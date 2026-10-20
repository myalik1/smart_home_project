#include <gtest/gtest.h>
#include "../include/RadiatorEmulator.h"
#include "../include/SmartRadiator.h"
#include "MockMessageSender.h"
#include <chrono>
#include <thread>

TEST(RadiatorEmulatorTest, SmoothlyOpensAndClosesValveOnValidCommands) {
    MockMessageSender sender;
    SmartRadiator radiator("Rad1", "R-01", "Radiator", "home/rad1");
    radiator.setSender(&sender);
    RadiatorEmulator emulator(&radiator, 15);

    radiator.onMessageReceived(R"({"command": "SET_VALVE_LEVEL", "value": 40})");

    emulator.tick();
    EXPECT_EQ(radiator.getCurrentValveLevel(), 15);
    emulator.tick();
    EXPECT_EQ(radiator.getCurrentValveLevel(), 30);
    emulator.tick();
    EXPECT_EQ(radiator.getCurrentValveLevel(), 40);

    int callsAtTarget = sender.callCount;
    emulator.tick();
    EXPECT_EQ(sender.callCount, callsAtTarget);

    radiator.onMessageReceived(R"({"command": "OFF"})");
    emulator.tick();
    EXPECT_EQ(radiator.getCurrentValveLevel(), 25);
    emulator.tick();
    EXPECT_EQ(radiator.getCurrentValveLevel(), 10);
    emulator.tick();
    EXPECT_EQ(radiator.getCurrentValveLevel(), 0);
}

TEST(RadiatorEmulatorTest, IgnoresInvalidCommandsAndRunsThreadLifecycle) {
    MockMessageSender sender;
    SmartRadiator radiator("Rad1", "R-01", "Radiator", "home/rad1");
    radiator.setSender(&sender);
    RadiatorEmulator emulator(&radiator, 20);

    radiator.onMessageReceived("{broken_json");
    radiator.onMessageReceived(R"({"command": "SET_TARGET_TEMP", "value": 25})");
    emulator.tick();
    EXPECT_EQ(radiator.getCurrentValveLevel(), 0);
    EXPECT_EQ(sender.callCount, 0);

    radiator.onMessageReceived(R"({"command": "SET_VALVE_LEVEL", "value": 50})");
    emulator.start();
    emulator.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    emulator.stop();
    EXPECT_GE(radiator.getCurrentValveLevel(), 20);
}
