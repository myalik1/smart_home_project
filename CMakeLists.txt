cmake_minimum_required(VERSION 3.16)

project(SmartHome)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PahoMqttCpp REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(Threads REQUIRED)

include_directories(include)

set(CORE_SOURCES
    src/SmartDevice.cpp src/MqttMessageSender.cpp
    src/SmartThermostat.cpp src/ThermostatEmulator.cpp
    src/SmartRadiator.cpp src/RadiatorEmulator.cpp
    src/SmartPlug.cpp src/PlugEmulator.cpp
    src/SmartHygrometer.cpp src/HygrometerEmulator.cpp
    src/SmartLamp.cpp
)

# Основное приложение
add_executable(SmartHomeApp
    main.cpp
    src/SmartDevice.cpp
    src/SmartLamp.cpp
    src/SmartThermostat.cpp
    src/SmartPlug.cpp
    src/SmartHygrometer.cpp
    src/SmartRadiator.cpp
    src/MqttMessageSender.cpp
    src/SQLiteStorage.cpp
    src/TelemetryHistoryManager.cpp
    src/DeviceManager.cpp
    src/OneShotTimer.cpp
    src/IntervalTimer.cpp
    src/RuleEngine.cpp
    src/RuleConfigParser.cpp
)
target_link_libraries(SmartHomeApp PRIVATE paho-mqttpp3 paho-mqtt3as SQLite::SQLite3 Threads::Threads)

# 1. Приложение Термостата
add_executable(app_thermostat apps/app_thermostat.cpp ${CORE_SOURCES})
target_link_libraries(app_thermostat PRIVATE paho-mqttpp3 paho-mqtt3as Threads::Threads)

# 2. Приложение Радиатора
add_executable(app_radiator apps/app_radiator.cpp ${CORE_SOURCES})
target_link_libraries(app_radiator PRIVATE paho-mqttpp3 paho-mqtt3as Threads::Threads)

# 3. Приложение Розетки
add_executable(app_plug apps/app_plug.cpp ${CORE_SOURCES})
target_link_libraries(app_plug PRIVATE paho-mqttpp3 paho-mqtt3as Threads::Threads)

# 4. Приложение Гигрометра
add_executable(app_hygrometer apps/app_hygrometer.cpp ${CORE_SOURCES})
target_link_libraries(app_hygrometer PRIVATE paho-mqttpp3 paho-mqtt3as Threads::Threads)

# 5. Приложение Лампы
add_executable(app_lamp apps/app_lamp.cpp ${CORE_SOURCES})
target_link_libraries(app_lamp PRIVATE paho-mqttpp3 paho-mqtt3as Threads::Threads)

# Временный тест MQTT (можно удалить позже)
add_executable(mqtt_test mqtt_test.cpp)
target_link_libraries(mqtt_test PRIVATE paho-mqttpp3 paho-mqtt3as)
    
enable_testing()

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
FetchContent_MakeAvailable(googletest)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")

add_executable(smart_home_tests
    tests/test_SmartLamp.cpp
    tests/test_SmartThermostat.cpp
    tests/test_SmartPlug.cpp
    tests/test_SmartHygrometer.cpp
    tests/test_SmartRadiator.cpp
    tests/test_HygrometerEmulator.cpp
    tests/test_PlugEmulator.cpp
    tests/test_RadiatorEmulator.cpp
    tests/test_ThermostatEmulator.cpp
    tests/test_SQLiteStorage.cpp
    tests/test_DeviceManager.cpp
    tests/test_TelemetryHistory.cpp
    tests/test_RuleEngine.cpp
    tests/test_RuleConfigParser.cpp
    tests/test_TimerCoverage.cpp
    tests/test_CommandRouting.cpp
    src/SmartLamp.cpp
    src/SmartThermostat.cpp
    src/SmartPlug.cpp
    src/SmartHygrometer.cpp
    src/SmartRadiator.cpp
    src/SmartDevice.cpp
    src/PlugEmulator.cpp
    src/RadiatorEmulator.cpp
    src/HygrometerEmulator.cpp
    src/ThermostatEmulator.cpp
    src/SQLiteStorage.cpp
    src/TelemetryHistoryManager.cpp
    src/DeviceManager.cpp
    src/OneShotTimer.cpp
    src/IntervalTimer.cpp
    src/RuleEngine.cpp
    src/RuleConfigParser.cpp
)

target_link_libraries(smart_home_tests gtest_main SQLite::SQLite3 Threads::Threads)

include(GoogleTest)
gtest_discover_tests(smart_home_tests)
