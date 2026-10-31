#include <gtest/gtest.h>
#include "../include/ThermostatEmulator.h"
#include "../include/RadiatorEmulator.h"
#include "../include/SmartThermostat.h"
#include "../include/SmartRadiator.h"
#include "../include/IMessageSender.h"
#include <chrono>
#include <thread>

class RoutingMockSender : public IMessageSender {
public:
    SmartRadiator* targetRadiator = nullptr;
    SmartThermostat* targetThermostat = nullptr;
    int callCount = 0;

    void sendMessage(const std::string& topic, const std::string& message) override {
        callCount++;
        if (topic == "home/radiator1/set" && targetRadiator != nullptr) {
            targetRadiator->onMessageReceived(message);
        }
        else if (topic == "home/radiator1" && targetThermostat != nullptr) {
            targetThermostat->onMessageReceived(message);
        }
    }
};

TEST(ThermostatEmulatorTest, ClosedLoopHeatingAndNaturalCoolingViaMqtt) {
    RoutingMockSender router;
    SmartRadiator radiator("Rad1", "R-01", "Radiator", "home/radiator1");
    SmartThermostat thermostat("Therm1", "T-01", "Thermostat", "home/therm1", "home/radiator1/set");

    router.targetRadiator = &radiator;
    router.targetThermostat = &thermostat;
    radiator.setSender(&router);
    thermostat.setSender(&router);

    RadiatorEmulator radEmu(&radiator, 50);
    ThermostatEmulator thermoEmu(&thermostat, 16.0f);

    EXPECT_FLOAT_EQ(thermostat.getCurrentTemperature(), 16.0f);

    thermostat.onMessageReceived(R"({"command": "SET_TEMPERATURE", "value": 22.0})");
    thermostat.onMessageReceived(R"({"command": "SET_MODE", "mode": "HEAT"})");
    EXPECT_EQ(radiator.getTargetValveLevel(), 100);

    radEmu.tick();
    radEmu.tick();
    EXPECT_EQ(radiator.getCurrentValveLevel(), 100);
    EXPECT_EQ(thermostat.getRadiatorValveLevel(), 100);

    thermoEmu.tick();
    EXPECT_GT(thermostat.getCurrentTemperature(), 16.0f);

    thermostat.onMessageReceived(R"({"command": "SET_MODE", "mode": "OFF"})");
    radEmu.tick();
    radEmu.tick();
    EXPECT_EQ(thermostat.getRadiatorValveLevel(), 0);

    float tempBeforeCooling = thermostat.getCurrentTemperature();
    thermoEmu.tick();
    EXPECT_LT(thermostat.getCurrentTemperature(), tempBeforeCooling);
}

TEST(ThermostatEmulatorTest, HandlesInvalidCommandsAndFastThreadStop) {
    RoutingMockSender router;
    SmartThermostat thermostat("Therm1", "T-01", "Thermostat", "home/therm1");
    thermostat.setSender(&router);

    ThermostatEmulator thermoEmu(&thermostat, 18.0f);

    thermostat.onMessageReceived("invalid_json");
    thermostat.onMessageReceived(R"({"command": "SET_MODE", "mode": "INVALID_MODE"})");
    thermoEmu.tick();
    EXPECT_FLOAT_EQ(thermostat.getCurrentTemperature(), 18.0f);

    thermoEmu.start();
    thermoEmu.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    thermoEmu.stop();
}
