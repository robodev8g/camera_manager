#include "camera_manager/camera_adapter.hpp"
#include "camera_manager/v4l2_camera_adapter.hpp"

#include <exception>
#include <iostream>
#include <memory>

int main() {
    try {
        std::unique_ptr<camera_manager::CameraAdapter> camera =
            std::make_unique<camera_manager::V4L2CameraAdapter>();
        camera->live_view();
    } catch (const std::exception& error) {
        std::cerr << "Camera error: " << error.what() << '\n';
        return 1;
    }
}
