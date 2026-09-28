cmake_minimum_required(VERSION 3.16)

project(simple_camera LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenCV 4 REQUIRED COMPONENTS core highgui videoio)

add_executable(simple-camera main.cpp)
target_include_directories(simple-camera PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(simple-camera PRIVATE ${OpenCV_LIBS})
target_compile_options(simple-camera PRIVATE -Wall -Wextra -Wpedantic)
