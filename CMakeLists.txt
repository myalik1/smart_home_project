cmake_minimum_required(VERSION 3.10)

project(SmartHome)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)
add_executable(SmartHomeApp main.cpp src/SmartDevice.cpp src/SmartLamp.cpp src/SmartThermostat.cpp src/SmartPlug.cpp)
