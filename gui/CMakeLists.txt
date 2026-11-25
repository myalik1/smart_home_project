cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(SmartHomeGui LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Charts)
find_package(Qt6 COMPONENTS WebSockets QUIET)

if(NOT Qt6WebSockets_FOUND)
    message(WARNING
        "Qt6::WebSockets was not found in ${Qt6_DIR}. "
        "SmartHomeGui will build without WebSocket support until Qt WebSockets is installed."
    )
endif()

set(GUI_SOURCES
    src/main.cpp
    src/MainWindow.cpp
    include/MainWindow.h
    ui/MainWindow.ui
    resources/resources.qrc
)

add_executable(SmartHomeGui WIN32 ${GUI_SOURCES})

set_target_properties(SmartHomeGui PROPERTIES
    AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/ui"
)

target_include_directories(SmartHomeGui PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(SmartHomeGui PRIVATE
    Qt6::Widgets
    Qt6::Network
    Qt6::Charts
    $<$<TARGET_EXISTS:Qt6::WebSockets>:Qt6::WebSockets>
)

target_compile_definitions(SmartHomeGui PRIVATE
    $<$<TARGET_EXISTS:Qt6::WebSockets>:SMARTHOME_HAS_QT_WEBSOCKETS=1>
)
