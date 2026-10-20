#include <gtest/gtest.h>
#include "../include/HygrometerEmulator.h"
#include "../include/SmartHygrometer.h"
#include "MockMessageSender.h"
#include <chrono>
#include <thread>

TEST(HygrometerEmulatorTest, RecoversFromHighAndLowHumidityAndDriftsNaturally) {
    MockMessageSender sender;
    SmartHygrometer hygro("Hygro1", "H-01", "Hygrometer", "home/hygro1");
    hygro.setSender(&sender);
    HygrometerEmulator emulator(&hygro, 45.0f);

    EXPECT_FLOAT_EQ(hygro.getCurrentHumidity(), 45.0f);

    hygro.onMessageReceived(R"({"command": "UPDATE_HUMIDITY", "value": 65.0})");
    emulator.tick();
    EXPECT_FLOAT_EQ(hygro.getCurrentHumidity(), 64.5f);

    hygro.onMessageReceived(R"({"command": "UPDATE_HUMIDITY", "value": 25.0})");
    emulator.tick();
    EXPECT_FLOAT_EQ(hygro.getCurrentHumidity(), 25.5f);

    hygro.onMessageReceived(R"({"command": "UPDATE_HUMIDITY", "value": 45.0})");
    for (int i = 0; i < 25; ++i) {
        emulator.tick();
        EXPECT_GE(hygro.getCurrentHumidity(), 43.5f);
        EXPECT_LE(hygro.getCurrentHumidity(), 46.5f);
    }
}

TEST(HygrometerEmulatorTest, IgnoresInvalidCommandsAndTestsThreadLifecycle) {
    MockMessageSender sender;
    SmartHygrometer hygro("Hygro1", "H-01", "Hygrometer", "home/hygro1");
    hygro.setSender(&sender);
    HygrometerEmulator emulator(&hygro, 50.0f);

    hygro.onMessageReceived("corrupted_json");
    hygro.onMessageReceived(R"({"command": "SET_HUMIDITY_WRONG", "value": 90})");
    EXPECT_FLOAT_EQ(hygro.getCurrentHumidity(), 50.0f);

    emulator.start();
    emulator.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    emulator.stop();
}
