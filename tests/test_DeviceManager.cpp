#include <gtest/gtest.h>

#include "../include/DeviceManager.h"
#include "../include/SmartLamp.h"
#include "../include/SmartPlug.h"
#include "../include/SmartThermostat.h"
#include "../include/SmartHygrometer.h"
#include "../include/SmartRadiator.h"
#include "../include/json.hpp"
#include "MockMqttClient.h"

using json = nlohmann::json;

class DeviceManagerTest : public ::testing::Test {
protected:
    MockMqttClient mockClient;
    std::unique_ptr<DeviceManager> manager;

    std::shared_ptr<SmartLamp> lamp;
    std::shared_ptr<SmartPlug> plug;
    std::shared_ptr<SmartHygrometer> hygro;

    void SetUp() override {
        manager = std::make_unique<DeviceManager>(&mockClient);

        lamp = std::make_shared<SmartLamp>("Lamp1", "L-01", "Lamp", "home/lamp1");
        plug = std::make_shared<SmartPlug>("Plug1", "P-01", "Plug", "home/plug1");
        hygro = std::make_shared<SmartHygrometer>("Hygro1", "H-01", "Hygrometer", "home/hygro1");
    }

    void TearDown() override {
        mockClient.clear();
    }
};

TEST_F(DeviceManagerTest, RegistersDevicesSuccessfully) {
    EXPECT_EQ(manager->getDeviceCount(), 0u);

    EXPECT_TRUE(manager->registerDevice(lamp));
    EXPECT_TRUE(manager->registerDevice(plug));
    EXPECT_EQ(manager->getDeviceCount(), 2u);

    EXPECT_TRUE(manager->hasDevice("Lamp1"));
    EXPECT_TRUE(manager->hasDevice("Plug1"));
    EXPECT_FALSE(manager->hasDevice("NonExistent"));

    EXPECT_EQ(manager->getDevice("Lamp1"), lamp);
    EXPECT_EQ(manager->getDevice("Plug1"), plug);
    EXPECT_EQ(manager->getDevice("NonExistent"), nullptr);
}

TEST_F(DeviceManagerTest, RejectsNullOrDuplicateDevice) {
    EXPECT_FALSE(manager->registerDevice(nullptr));

    EXPECT_TRUE(manager->registerDevice(lamp));
    EXPECT_FALSE(manager->registerDevice(lamp));
    EXPECT_EQ(manager->getDeviceCount(), 1u);
}

TEST_F(DeviceManagerTest, UnregisterDevice) {
    manager->registerDevice(lamp);
    manager->registerDevice(plug);
    EXPECT_EQ(manager->getDeviceCount(), 2u);

    EXPECT_TRUE(manager->unregisterDevice("Lamp1"));
    EXPECT_FALSE(manager->hasDevice("Lamp1"));
    EXPECT_EQ(manager->getDeviceCount(), 1u);

    EXPECT_FALSE(manager->unregisterDevice("Lamp1"));
}

TEST_F(DeviceManagerTest, GetAllDevices) {
    manager->registerDevice(lamp);
    manager->registerDevice(plug);

    auto all = manager->getAllDevices();
    EXPECT_EQ(all.size(), 2u);
}

TEST_F(DeviceManagerTest, AutomaticallySubscribesToTopicsOnRegistration) {
    manager->registerDevice(lamp);

    EXPECT_TRUE(mockClient.isSubscribed("home/lamp1/set"));
    EXPECT_TRUE(mockClient.isSubscribed("home/lamp1"));
}

TEST_F(DeviceManagerTest, LateClientAttachmentSubscribesAllDevices) {
    DeviceManager lateManager(nullptr);
    lateManager.registerDevice(lamp);
    lateManager.registerDevice(plug);

    EXPECT_FALSE(mockClient.isSubscribed("home/lamp1/set"));

    lateManager.setMqttClient(&mockClient);

    EXPECT_TRUE(mockClient.isSubscribed("home/lamp1/set"));
    EXPECT_TRUE(mockClient.isSubscribed("home/plug1/set"));
}

TEST_F(DeviceManagerTest, RoutesIncomingBrokerMessageToDevice) {
    manager->registerDevice(lamp);

    bool handled = mockClient.simulateIncomingMessage("home/lamp1/set", R"({"command": "ON"})");
    EXPECT_TRUE(handled);

    ASSERT_FALSE(mockClient.lastMessage.empty());
    json status = json::parse(mockClient.lastMessage);
    EXPECT_EQ(status["deviceName"], "Lamp1");
    EXPECT_EQ(status["isOn"], true);
}

TEST_F(DeviceManagerTest, DispatchesBrightnessCommandToLamp) {
    manager->registerDevice(lamp);

    mockClient.simulateIncomingMessage("home/lamp1/set", R"({"command": "SET_BRIGHTNESS", "value": 42})");

    ASSERT_FALSE(mockClient.lastMessage.empty());
    json status = json::parse(mockClient.lastMessage);
    EXPECT_EQ(status["brightness"], 42);
}

TEST_F(DeviceManagerTest, SendCommandRoutesToCorrectTopic) {
    manager->registerDevice(lamp);
    manager->registerDevice(plug);

    EXPECT_TRUE(manager->sendCommand("Lamp1", R"({"command": "OFF"})"));
    EXPECT_EQ(mockClient.lastTopic, "home/lamp1/set");
    EXPECT_EQ(mockClient.lastMessage, R"({"command": "OFF"})");

    EXPECT_TRUE(manager->sendCommand("Plug1", R"({"command": "ON"})"));
    EXPECT_EQ(mockClient.lastTopic, "home/plug1/set");
    EXPECT_EQ(mockClient.lastMessage, R"({"command": "ON"})");

    EXPECT_FALSE(manager->sendCommand("GhostDevice", R"({"command": "ON"})"));
}

TEST_F(DeviceManagerTest, CollectsStateUpdatesAndTriggersListeners) {
    manager->registerDevice(hygro);

    std::string notifiedDevice;
    std::string notifiedPayload;
    int callbackCount = 0;

    manager->onStateUpdate([&](const std::string& devName, const std::string& payload) {
        notifiedDevice = devName;
        notifiedPayload = payload;
        callbackCount++;
    });

    const std::string sampleState = R"({"deviceName":"Hygro1","currentHumidity":55.0})";

    mockClient.simulateIncomingMessage("home/hygro1", sampleState);

    EXPECT_EQ(callbackCount, 1);
    EXPECT_EQ(notifiedDevice, "Hygro1");
    EXPECT_EQ(notifiedPayload, sampleState);

    EXPECT_EQ(manager->getLatestState("Hygro1"), sampleState);
}

TEST_F(DeviceManagerTest, PublishAllStates) {
    manager->registerDevice(lamp);
    manager->registerDevice(plug);

    mockClient.sentMessages.clear();

    manager->publishAllStates();

    EXPECT_EQ(mockClient.sentMessages.size(), 2u);
}
