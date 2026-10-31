#include <gtest/gtest.h>

#include "../include/DeviceManager.h"
#include "../include/SmartLamp.h"
#include "../include/SmartPlug.h"
#include "../include/SmartThermostat.h"
#include "../include/SmartHygrometer.h"
#include "../include/SmartRadiator.h"
#include "../include/SQLiteStorage.h"
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

TEST_F(DeviceManagerTest, ParsesTelemetryFromMultipleSimulators) {
    manager->registerDevice(lamp);
    manager->registerDevice(plug);
    manager->registerDevice(hygro);

    const std::string plugState = R"({
        "deviceName": "Plug1",
        "deviceType": "Plug",
        "serialNumber": "P-01",
        "isOn": true,
        "currentPower": 1995.5
    })";

    const std::string hygroState = R"({
        "deviceName": "Hygro1",
        "deviceType": "Hygrometer",
        "serialNumber": "H-01",
        "currentHumidity": 46.2
    })";

    mockClient.simulateIncomingMessage("home/plug1", plugState);
    mockClient.simulateIncomingMessage("home/hygro1", hygroState);

    auto plugTele = manager->getTelemetry("Plug1");
    ASSERT_TRUE(plugTele.has_value());
    EXPECT_EQ(plugTele->deviceName, "Plug1");
    EXPECT_EQ(plugTele->deviceType, "Plug");
    EXPECT_TRUE(plugTele->isOn);
    EXPECT_FLOAT_EQ(plugTele->currentPower, 1995.5f);
    EXPECT_DOUBLE_EQ(plugTele->getMetric("currentPower"), 1995.5);

    auto hygroTele = manager->getTelemetry("Hygro1");
    ASSERT_TRUE(hygroTele.has_value());
    EXPECT_EQ(hygroTele->deviceName, "Hygro1");
    EXPECT_FLOAT_EQ(hygroTele->currentHumidity, 46.2f);
}

TEST_F(DeviceManagerTest, TracksActiveSimulatorsAndTimeout) {
    manager->registerDevice(lamp);
    manager->registerDevice(plug);

    EXPECT_FALSE(manager->isSimulatorActive("Lamp1", 1));
    EXPECT_FALSE(manager->isSimulatorActive("Plug1", 1));
    EXPECT_EQ(manager->getActiveSimulators(1).size(), 0u);

    mockClient.simulateIncomingMessage("home/plug1", R"({"deviceName":"Plug1","deviceType":"Plug","isOn":true,"currentPower":150.0})");

    EXPECT_TRUE(manager->isSimulatorActive("Plug1", 5));
    EXPECT_FALSE(manager->isSimulatorActive("Lamp1", 5));

    auto activeList = manager->getActiveSimulators(5);
    ASSERT_EQ(activeList.size(), 1u);
    EXPECT_EQ(activeList[0], "Plug1");
}

TEST_F(DeviceManagerTest, CollectsActiveTelemetryAcrossAllSimulators) {
    manager->registerDevice(lamp);
    manager->registerDevice(plug);

    mockClient.simulateIncomingMessage("home/lamp1", R"({"deviceName":"Lamp1","deviceType":"Lamp","isOn":true,"brightness":80})");
    mockClient.simulateIncomingMessage("home/plug1", R"({"deviceName":"Plug1","deviceType":"Plug","isOn":false,"currentPower":0.0})");

    auto activeTelemetries = manager->collectActiveTelemetry(10);
    EXPECT_EQ(activeTelemetries.size(), 2u);

    auto allTelemetries = manager->getAllTelemetry();
    EXPECT_EQ(allTelemetries.size(), 2u);
}

TEST_F(DeviceManagerTest, TriggersTelemetryCallbackWithParsedStructure) {
    manager->registerDevice(hygro);

    DeviceTelemetry receivedTelemetry;
    int callbacksFired = 0;

    manager->onTelemetryReceived([&](const DeviceTelemetry& t) {
        receivedTelemetry = t;
        callbacksFired++;
    });

    const std::string payload = R"({"deviceName":"Hygro1","deviceType":"Hygrometer","currentHumidity":52.3})";
    mockClient.simulateIncomingMessage("home/hygro1", payload);

    EXPECT_EQ(callbacksFired, 1);
    EXPECT_EQ(receivedTelemetry.deviceName, "Hygro1");
    EXPECT_EQ(receivedTelemetry.deviceType, "Hygrometer");
    EXPECT_FLOAT_EQ(receivedTelemetry.currentHumidity, 52.3f);
    EXPECT_EQ(receivedTelemetry.rawJson, payload);
}

TEST_F(DeviceManagerTest, DynamicallyCollectsFromExternalSimulatorsWithoutPreRegistration) {
    manager->enableWildcardCollection("home/#");

    const std::string extPayload = R"({
        "deviceName": "ExtThermostat",
        "deviceType": "Thermostat",
        "serialNumber": "EXT-01",
        "currentTemperature": 21.5,
        "targetTemperature": 23.0,
        "mode": "HEAT"
    })";

    mockClient.simulateIncomingMessage("home/external_thermo", extPayload);

    EXPECT_TRUE(manager->isSimulatorActive("ExtThermostat", 5));
    auto tele = manager->getTelemetry("ExtThermostat");
    ASSERT_TRUE(tele.has_value());
    EXPECT_EQ(tele->deviceName, "ExtThermostat");
    EXPECT_EQ(tele->deviceType, "Thermostat");
    EXPECT_FLOAT_EQ(tele->currentTemperature, 21.5f);
    EXPECT_FLOAT_EQ(tele->targetTemperature, 23.0f);
    EXPECT_EQ(tele->mode, "HEAT");
}

TEST_F(DeviceManagerTest, IgnoresInvalidOrBrokenJsonTelemetryGracefully) {
    manager->registerDevice(lamp);

    bool received = false;
    manager->onTelemetryReceived([&](const DeviceTelemetry&) {
        received = true;
    });

    mockClient.simulateIncomingMessage("home/lamp1", "{this is not json");
    EXPECT_FALSE(received);
    EXPECT_FALSE(manager->isSimulatorActive("Lamp1", 5));

    mockClient.simulateIncomingMessage("home/lamp1", "[1, 2, 3]");
    EXPECT_FALSE(received);
}

TEST_F(DeviceManagerTest, WildcardAndDirectSubscriptionDoesNotDuplicateTelemetryOrHistory) {
    manager->registerDevice(lamp);

    auto storage = std::make_shared<SQLiteStorage>();
    ASSERT_TRUE(storage->open(":memory:"));
    manager->attachStorage(storage);

    // Enable wildcard while device is already registered
    manager->enableWildcardCollection("home/#");

    int telemetryCallbacksFired = 0;
    manager->onTelemetryReceived([&](const DeviceTelemetry&) {
        telemetryCallbacksFired++;
    });

    const std::string payload = R"({"deviceName":"Lamp1","deviceType":"Lamp","isOn":true,"brightness":70})";

    // Simulate incoming message that matches both home/lamp1 and home/#
    mockClient.simulateIncomingMessage("home/lamp1", payload);

    // Must be delivered exactly once despite matching both subscriptions
    EXPECT_EQ(telemetryCallbacksFired, 1);

    auto history = manager->getDeviceHistory("Lamp1");
    EXPECT_EQ(history.size(), 1u);
}
