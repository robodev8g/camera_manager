add_executable(camera_backend_test
    camera_backend_test.cpp
)

target_link_libraries(camera_backend_test
    PRIVATE
        camera_agent::backend
)

add_test(NAME camera_backend_test COMMAND camera_backend_test)
