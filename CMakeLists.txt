cmake_minimum_required(VERSION 3.24)

project(camera_manager VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CAMERA_MANAGER_BUILD_TESTS "Build camera-manager tests" ON)
option(CAMERA_MANAGER_BUILD_TOOLS "Build camera-manager command-line tools" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(agent)

if(CAMERA_MANAGER_BUILD_TESTS)
    include(CTest)
    enable_testing()
    add_subdirectory(agent/tests)
endif()
