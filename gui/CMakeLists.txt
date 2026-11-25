cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(SmartHomeGui LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    enable_testing()
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(BUILD_GUI_TESTS "Build Qt 6 GUI unit tests" ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network WebSockets Charts)

add_library(SmartHomeGuiNet STATIC
    include/net/DtoModels.h
    include/net/ApiClient.h
    include/net/WsClient.h
    src/net/DtoModels.cpp
    src/net/ApiClient.cpp
    src/net/WsClient.cpp
)

target_include_directories(SmartHomeGuiNet PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(SmartHomeGuiNet PUBLIC
    Qt6::Core
    Qt6::Network
    Qt6::WebSockets
)

add_executable(SmartHomeGui WIN32
    src/main.cpp
    src/MainWindow.cpp
    include/MainWindow.h
    ui/MainWindow.ui
    resources/resources.qrc
)

set_target_properties(SmartHomeGui PROPERTIES
    AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/ui"
)

target_link_libraries(SmartHomeGui PRIVATE
    SmartHomeGuiNet
    Qt6::Widgets
    Qt6::Charts
)

if(BUILD_GUI_TESTS)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(smart_home_gui_tests
        tests/test_NetworkLayer.cpp
    )
    target_link_libraries(smart_home_gui_tests PRIVATE
        SmartHomeGuiNet
        Qt6::Test
    )
    add_test(NAME smart_home_gui_tests COMMAND smart_home_gui_tests)
endif()
