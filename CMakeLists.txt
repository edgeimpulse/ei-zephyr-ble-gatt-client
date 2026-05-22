cmake_minimum_required(VERSION 3.20.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ei-zephyr-ble-gatt-client)

# Colorful diagnostics
zephyr_compile_options(-fdiagnostics-color=always)

# Add include directories
target_include_directories(app PRIVATE
    src
    src/ble
)

# Add application sources
target_sources(app PRIVATE
    src/main.cpp
    src/ble/gatt_client.cpp
    src/ble/gatt_server.cpp
)
