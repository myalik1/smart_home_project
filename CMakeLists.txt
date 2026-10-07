cmake_minimum_required(VERSION 3.16)

project(SmartHome)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PahoMqttCpp REQUIRED)

include_directories(include)

add_executable(SmartHomeApp
    main.cpp
    src/SmartDevice.cpp
    src/SmartLamp.cpp
)

target_link_libraries(SmartHomeApp PRIVATE paho-mqttpp3 paho-mqtt3as)

add_executable(mqtt_test mqtt_test.cpp)
target_link_libraries(mqtt_test PRIVATE paho-mqttpp3 paho-mqtt3as)