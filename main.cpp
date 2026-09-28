#include "camera_cli.hpp"
#include "v4l2_camera_adapter.hpp"

#include <exception>
#include <filesystem>
#include <iostream>

namespace {
const std::filesystem::path media_dir_path{"media"};
}

int main() {
    try {
        camera_manager::V4L2CameraAdapter camera;
        camera_manager::CameraCli cli(camera, media_dir_path);
        cli.run();
    } catch (const std::exception& error) {
        std::cerr << "Camera error: " << error.what() << '\n';
        return 1;
    }
}
