cmake_minimum_required(VERSION 3.20)
project(CCSCharger VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS Widgets Charts Core Gui)

# Platform detection
if(WIN32)
    add_compile_definitions(PLATFORM_WINDOWS)
elseif(UNIX)
    add_compile_definitions(PLATFORM_LINUX)
endif()

# ─── CAN Layer ────────────────────────────────────────────
add_library(can_layer STATIC
    src/can/can_frame.h
    src/can/can_interface.h
    src/can/can_interface.cpp
    src/can/pcan_driver.h
    src/can/pcan_driver.cpp
)
target_include_directories(can_layer PUBLIC src)
target_link_libraries(can_layer PUBLIC Qt6::Core)
if(WIN32)
    # PCAN-Basic DLL loaded dynamically at runtime
elseif(UNIX)
    target_link_libraries(can_layer PRIVATE dl)
endif()

# ─── DBC Layer ────────────────────────────────────────────
add_library(dbc_layer STATIC
    src/dbc/dbc_parser.h
    src/dbc/dbc_parser.cpp
    src/dbc/signal_codec.h
    src/dbc/signal_codec.cpp
)
target_include_directories(dbc_layer PUBLIC src)
target_link_libraries(dbc_layer PUBLIC Qt6::Core)

# ─── Module Layer ─────────────────────────────────────────
add_library(module_layer STATIC
    src/module/charge_module.h
    src/module/charge_module.cpp
    src/module/state_machine.h
    src/module/state_machine.cpp
    src/module/safety_monitor.h
    src/module/safety_monitor.cpp
)
target_include_directories(module_layer PUBLIC src)
target_link_libraries(module_layer PUBLIC can_layer dbc_layer Qt6::Core)

# ─── Logging Layer ────────────────────────────────────────
add_library(logging_layer STATIC
    src/logging/can_logger.h
    src/logging/can_logger.cpp
    src/logging/session_report.h
    src/logging/session_report.cpp
)
target_include_directories(logging_layer PUBLIC src)
target_link_libraries(logging_layer PUBLIC Qt6::Core can_layer dbc_layer)

# ─── UI Layer ─────────────────────────────────────────────
add_library(ui_layer STATIC
    src/ui/mainwindow.h
    src/ui/mainwindow.cpp
    src/ui/dashboard_widget.h
    src/ui/dashboard_widget.cpp
    src/ui/expert_widget.h
    src/ui/expert_widget.cpp
    src/ui/connection_widget.h
    src/ui/connection_widget.cpp
    src/ui/chart_widget.h
    src/ui/chart_widget.cpp
    src/ui/theme.h
    src/ui/theme.cpp
)
target_include_directories(ui_layer PUBLIC src)
target_link_libraries(ui_layer PUBLIC
    Qt6::Widgets Qt6::Charts Qt6::Core Qt6::Gui
    module_layer logging_layer can_layer dbc_layer
)

# ─── Main Executable ─────────────────────────────────────
add_executable(CCSCharger src/main.cpp)
target_link_libraries(CCSCharger PRIVATE ui_layer)
