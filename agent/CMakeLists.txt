find_package(OpenCV 4 REQUIRED COMPONENTS core highgui imgcodecs videoio)
find_package(Threads REQUIRED)

add_library(camera_agent_backend
    src/backends/opencv_camera_backend.cpp
    src/backends/v4l2_controls.cpp
)

add_library(camera_agent::backend ALIAS camera_agent_backend)

target_include_directories(camera_agent_backend
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(camera_agent_backend
    PUBLIC
        ${OpenCV_LIBS}
    PRIVATE
        Threads::Threads
)

target_compile_features(camera_agent_backend PUBLIC cxx_std_20)

target_compile_options(camera_agent_backend
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
)

add_executable(camera-agent
    src/main.cpp
    src/agent_config.cpp
    src/camera_controller.cpp
    src/local_cli.cpp
    src/udp_agent_server.cpp
)
target_link_libraries(camera-agent PRIVATE camera_agent::backend)
target_compile_options(camera-agent PRIVATE -Wall -Wextra -Wpedantic)

if(CAMERA_MANAGER_BUILD_TOOLS)
    add_executable(camera-backend-smoke
        tools/camera_backend_smoke.cpp
    )
    target_link_libraries(camera-backend-smoke PRIVATE camera_agent::backend)
endif()
