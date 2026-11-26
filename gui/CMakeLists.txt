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

add_library(SmartHomeGuiUi STATIC
    include/MainWindow.h
    include/widgets/AddRuleDialog.h
    include/widgets/AnalyticsWidget.h
    include/widgets/DashboardWidget.h
    include/widgets/DeviceCardWidget.h
    include/widgets/RulesAndSettingsWidget.h
    src/MainWindow.cpp
    src/widgets/AddRuleDialog.cpp
    src/widgets/AnalyticsWidget.cpp
    src/widgets/DashboardWidget.cpp
    src/widgets/DeviceCardWidget.cpp
    src/widgets/RulesAndSettingsWidget.cpp
    ui/MainWindow.ui
)

set_target_properties(SmartHomeGuiUi PROPERTIES
    AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/ui"
)

target_include_directories(SmartHomeGuiUi PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(SmartHomeGuiUi PUBLIC
    SmartHomeGuiNet
    Qt6::Widgets
    Qt6::Charts
)

add_executable(SmartHomeGui WIN32
    src/main.cpp
    resources/resources.qrc
)

target_link_libraries(SmartHomeGui PRIVATE
    SmartHomeGuiUi
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

    add_executable(smart_home_dashboard_tests
        tests/test_Dashboard.cpp
        resources/resources.qrc
    )
    target_link_libraries(smart_home_dashboard_tests PRIVATE
        SmartHomeGuiUi
        Qt6::Test
    )
    add_test(NAME smart_home_dashboard_tests COMMAND smart_home_dashboard_tests)
endif()
