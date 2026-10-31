#include <gtest/gtest.h>

#include "../include/TelemetryHistoryManager.h"
#include "../include/SQLiteStorage.h"
#include "../include/DeviceManager.h"
#include "../include/SmartPlug.h"
#include "../include/SmartHygrometer.h"
#include "MockMqttClient.h"

#include <filesystem>
#include <thread>
#include <chrono>

class TelemetryHistoryTest : public ::testing::Test {
protected:
    std::string dbPath;
    std::shared_ptr<SQLiteStorage> storage;
    std::unique_ptr<TelemetryHistoryManager> historyMgr;

    void SetUp() override {
        dbPath = "test_telemetry_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".db";
        storage = std::make_shared<SQLiteStorage>();
        ASSERT_TRUE(storage->open(dbPath));
        historyMgr = std::make_unique<TelemetryHistoryManager>(storage);
        ASSERT_TRUE(historyMgr->initSchema());
    }

    void TearDown() override {
        historyMgr.reset();
        if (storage && storage->isOpen()) {
            storage->close();
        }
        storage.reset();
        std::error_code ec;
        std::filesystem::remove(dbPath, ec);
    }
};

TEST_F(TelemetryHistoryTest, InitSchemaIsIdempotent) {
    EXPECT_TRUE(historyMgr->initSchema());
    EXPECT_TRUE(historyMgr->initSchema());
}

TEST_F(TelemetryHistoryTest, SaveAndRetrieveSingleDeviceTelemetry) {
    DeviceTelemetry t;
    t.deviceName = "Plug1";
    t.deviceType = "Plug";
    t.serialNumber = "PLUG-001";
    t.timestamp = std::chrono::system_clock::now();
    t.isOn = true;
    t.currentPower = 1850.5f;
    t.rawJson = R"({"deviceName":"Plug1","isOn":true,"currentPower":1850.5})";

    EXPECT_TRUE(historyMgr->saveTelemetry(t));
    EXPECT_EQ(historyMgr->getHistoryCount("Plug1"), 1u);
    EXPECT_EQ(historyMgr->getHistoryCount(), 1u);

    auto history = historyMgr->getHistory("Plug1", 10);
    ASSERT_EQ(history.size(), 1u);
    EXPECT_EQ(history[0].deviceName, "Plug1");
    EXPECT_EQ(history[0].deviceType, "Plug");
    EXPECT_EQ(history[0].serialNumber, "PLUG-001");
    EXPECT_TRUE(history[0].isOn);
    EXPECT_FLOAT_EQ(history[0].currentPower, 1850.5f);
    EXPECT_EQ(history[0].rawJson, t.rawJson);

    auto latest = historyMgr->getLatestRecorded("Plug1");
    ASSERT_TRUE(latest.has_value());
    EXPECT_EQ(latest->deviceName, "Plug1");
    EXPECT_FLOAT_EQ(latest->currentPower, 1850.5f);
}

TEST_F(TelemetryHistoryTest, OrdersByTimestampDescendingAndRespectsLimit) {
    auto baseTime = std::chrono::system_clock::now();

    for (int i = 1; i <= 5; ++i) {
        DeviceTelemetry t;
        t.deviceName = "Hygro1";
        t.deviceType = "Hygrometer";
        t.timestamp = baseTime + std::chrono::seconds(i);
        t.currentHumidity = 40.0f + static_cast<float>(i);
        t.rawJson = R"({"humidity":)" + std::to_string(t.currentHumidity) + "}";
        EXPECT_TRUE(historyMgr->saveTelemetry(t));
    }

    EXPECT_EQ(historyMgr->getHistoryCount("Hygro1"), 5u);

    auto top3 = historyMgr->getHistory("Hygro1", 3);
    ASSERT_EQ(top3.size(), 3u);
    // Newest first: 45%, 44%, 43%
    EXPECT_FLOAT_EQ(top3[0].currentHumidity, 45.0f);
    EXPECT_FLOAT_EQ(top3[1].currentHumidity, 44.0f);
    EXPECT_FLOAT_EQ(top3[2].currentHumidity, 43.0f);
}

TEST_F(TelemetryHistoryTest, QueriesHistoryByTimeRange) {
    auto t0 = std::chrono::system_clock::now();
    int64_t t0Ms = std::chrono::duration_cast<std::chrono::milliseconds>(t0.time_since_epoch()).count();

    DeviceTelemetry t1;
    t1.deviceName = "Therm1";
    t1.deviceType = "Thermostat";
    t1.timestamp = t0;
    t1.currentTemperature = 20.0f;
    t1.rawJson = "{}";
    historyMgr->saveTelemetry(t1);

    DeviceTelemetry t2;
    t2.deviceName = "Therm1";
    t2.deviceType = "Thermostat";
    t2.timestamp = t0 + std::chrono::milliseconds(200);
    t2.currentTemperature = 21.0f;
    t2.rawJson = "{}";
    historyMgr->saveTelemetry(t2);

    DeviceTelemetry t3;
    t3.deviceName = "Therm1";
    t3.deviceType = "Thermostat";
    t3.timestamp = t0 + std::chrono::milliseconds(400);
    t3.currentTemperature = 22.0f;
    t3.rawJson = "{}";
    historyMgr->saveTelemetry(t3);

    auto range = historyMgr->getHistoryRange("Therm1", t0Ms + 100, t0Ms + 300);
    ASSERT_EQ(range.size(), 1u);
    EXPECT_FLOAT_EQ(range[0].currentTemperature, 21.0f);
}

TEST_F(TelemetryHistoryTest, ClearsHistoryForSpecificDeviceAndAll) {
    DeviceTelemetry t1;
    t1.deviceName = "DevA";
    t1.deviceType = "TypeA";
    t1.rawJson = "{}";
    historyMgr->saveTelemetry(t1);

    DeviceTelemetry t2;
    t2.deviceName = "DevB";
    t2.deviceType = "TypeB";
    t2.rawJson = "{}";
    historyMgr->saveTelemetry(t2);

    EXPECT_EQ(historyMgr->getHistoryCount(), 2u);

    EXPECT_TRUE(historyMgr->clearHistory("DevA"));
    EXPECT_EQ(historyMgr->getHistoryCount("DevA"), 0u);
    EXPECT_EQ(historyMgr->getHistoryCount("DevB"), 1u);

    EXPECT_TRUE(historyMgr->clearHistory());
    EXPECT_EQ(historyMgr->getHistoryCount(), 0u);
}

TEST_F(TelemetryHistoryTest, EndToEndIntegrationWithDeviceManager) {
    MockMqttClient mockClient;
    DeviceManager manager(&mockClient);

    EXPECT_TRUE(manager.attachStorage(storage));
    ASSERT_NE(manager.getHistoryManager(), nullptr);

    auto plug = std::make_shared<SmartPlug>("Plug1", "P-01", "Plug", "home/plug1");
    auto hygro = std::make_shared<SmartHygrometer>("Hygro1", "H-01", "Hygrometer", "home/hygro1");

    manager.registerDevice(plug);
    manager.registerDevice(hygro);

    // Simulate telemetry messages from simulators over MQTT
    mockClient.simulateIncomingMessage("home/plug1", R"({"deviceName":"Plug1","deviceType":"Plug","isOn":true,"currentPower":1990.0})");
    mockClient.simulateIncomingMessage("home/plug1", R"({"deviceName":"Plug1","deviceType":"Plug","isOn":true,"currentPower":2005.0})");
    mockClient.simulateIncomingMessage("home/hygro1", R"({"deviceName":"Hygro1","deviceType":"Hygrometer","currentHumidity":55.0})");

    auto plugHistory = manager.getDeviceHistory("Plug1");
    ASSERT_EQ(plugHistory.size(), 2u);
    EXPECT_FLOAT_EQ(plugHistory[0].currentPower, 2005.0f);
    EXPECT_FLOAT_EQ(plugHistory[1].currentPower, 1990.0f);

    auto hygroHistory = manager.getDeviceHistory("Hygro1");
    ASSERT_EQ(hygroHistory.size(), 1u);
    EXPECT_FLOAT_EQ(hygroHistory[0].currentHumidity, 55.0f);

    EXPECT_EQ(manager.getHistoryManager()->getHistoryCount(), 3u);
}

TEST_F(TelemetryHistoryTest, HandlesClosedDatabaseGracefully) {
    storage->close();
    DeviceTelemetry t;
    t.deviceName = "Dev1";
    t.rawJson = "{}";

    EXPECT_FALSE(historyMgr->saveTelemetry(t));
    EXPECT_TRUE(historyMgr->getHistory("Dev1").empty());
    EXPECT_EQ(historyMgr->getHistoryCount(), 0u);
}
