cmake_minimum_required(VERSION 3.16)

project(camera_manager LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenCV 4 REQUIRED COMPONENTS core highgui imgcodecs videoio)
find_package(Threads REQUIRED)
find_package(jsoncpp CONFIG REQUIRED)

add_library(camera-adapter
    src/v4l2_camera_adapter.cpp
)
target_include_directories(camera-adapter
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
    PUBLIC ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(camera-adapter PRIVATE ${OpenCV_LIBS} Threads::Threads)
target_compile_options(camera-adapter PRIVATE -Wall -Wextra -Wpedantic)

add_executable(camera-manager
    main.cpp
    src/app_config.cpp
    src/cli_command_source.cpp
    src/command_executor.cpp
    src/command_handler.cpp
    src/command_queue.cpp
    src/tcp_command_source.cpp
    src/zmq_command_source.cpp
)
find_package(PkgConfig)
pkg_check_modules(PC_LIBZMQ QUIET libzmq)
if(PC_LIBZMQ_FOUND)
    message(STATUS "Found libzmq via pkg-config")
    add_compile_definitions(HAVE_LIBZMQ)
    include_directories(${PC_LIBZMQ_INCLUDE_DIRS})
    link_directories(${PC_LIBZMQ_LIBRARY_DIRS})
endif()

target_link_libraries(camera-manager PRIVATE camera-adapter JsonCpp::JsonCpp ${PC_LIBZMQ_LIBRARIES})
target_compile_options(camera-manager PRIVATE -Wall -Wextra -Wpedantic)
