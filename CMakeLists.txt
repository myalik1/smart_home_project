cmake_minimum_required(VERSION 3.16)

project(SmartHome)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PahoMqttCpp REQUIRED)
find_package(SQLite3 REQUIRED)

include_directories(include)

add_executable(SmartHomeApp
    main.cpp
    src/SmartDevice.cpp
    src/SmartLamp.cpp
    src/SQLiteStorage.cpp
)

target_link_libraries(SmartHomeApp PRIVATE
    paho-mqttpp3
    paho-mqtt3as
    SQLite::SQLite3
)

add_executable(mqtt_test mqtt_test.cpp)
target_link_libraries(mqtt_test PRIVATE paho-mqttpp3 paho-mqtt3as)

# Тест SQLiteStorage
add_executable(sqlite_test tests/sqlite_test.cpp src/SQLiteStorage.cpp)
target_link_libraries(sqlite_test PRIVATE SQLite::SQLite3)