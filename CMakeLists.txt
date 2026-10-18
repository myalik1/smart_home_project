cmake_minimum_required(VERSION 3.16)

project(SmartHome)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PahoMqttCpp REQUIRED)
find_package(SQLite3 REQUIRED)

include_directories(include)

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
)
target_link_libraries(SmartHomeApp PRIVATE paho-mqttpp3 paho-mqtt3as SQLite::SQLite3)

# Временный тест MQTT (можно удалить позже)
add_executable(mqtt_test mqtt_test.cpp)
target_link_libraries(mqtt_test PRIVATE paho-mqttpp3 paho-mqtt3as)

# Тесты (от коллег из feature/smart-device)
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
    tests/test_SQLiteStorage.cpp
    src/SmartLamp.cpp
    src/SmartThermostat.cpp
    src/SmartPlug.cpp
    src/SmartHygrometer.cpp
    src/SmartRadiator.cpp
    src/SmartDevice.cpp
    src/SQLiteStorage.cpp
)

target_link_libraries(smart_home_tests gtest_main SQLite::SQLite3)

include(GoogleTest)
gtest_discover_tests(smart_home_tests)
