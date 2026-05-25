cmake_minimum_required(VERSION 3.20.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# EI model — export from EI Studio → Zephyr library, unzip next to this project
# so that ../model/CMakeLists.txt exists, then re-run `west build`.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../model/CMakeLists.txt)
    list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/../model)
    message(STATUS "EI model module: ${CMAKE_CURRENT_SOURCE_DIR}/../model")
else()
    message(STATUS "No EI model found — sensor streaming only (no local inference).")
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ei-zephyr-ble-gatt-client)

# Colorful diagnostics
zephyr_compile_options(-fdiagnostics-color=always)

# Avoid pulling in __cxa_guard_acquire/release (not provided by minimal C++ runtime)
zephyr_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fno-threadsafe-statics>)

# Add include directories
target_include_directories(app PRIVATE
    src
    src/ble
    src/sensors
)

# Add application sources
target_sources(app PRIVATE
    src/main.cpp
    src/ble/gatt_client.cpp
    src/ble/gatt_server.cpp
    src/sensors/ei_sensor.cpp
)
