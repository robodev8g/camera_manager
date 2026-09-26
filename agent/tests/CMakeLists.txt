add_executable(camera_backend_test
    camera_backend_test.cpp
)

target_link_libraries(camera_backend_test
    PRIVATE
        camera_agent::backend
)

add_test(NAME camera_backend_test COMMAND camera_backend_test)

add_executable(camera_controller_test
    camera_controller_test.cpp
    ../src/camera_controller.cpp
)

target_link_libraries(camera_controller_test
    PRIVATE
        camera_agent::backend
)

add_test(NAME camera_controller_test COMMAND camera_controller_test)
