cmake_minimum_required(VERSION 3.27.9)

project(fwc_simulator VERSION 1.0.0)

find_package(Geode REQUIRED)

add_library(${PROJECT_NAME} SHARED
    src/main.cpp
    src/ReplayFormat.cpp
    src/FWCSimulator.cpp
)

setup_geode_mod(${PROJECT_NAME})
