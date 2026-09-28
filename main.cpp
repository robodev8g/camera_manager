#include "camera_cli.hpp"
#include "v4l2_camera_adapter.hpp"

#include <exception>
#include <iostream>

int main() {
    try {
        camera_manager::V4L2CameraAdapter camera;
        camera_manager::CameraCli cli(camera);
        cli.run();
    } catch (const std::exception& error) {
        std::cerr << "Camera error: " << error.what() << '\n';
        return 1;
    }
}
