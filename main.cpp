#include "app_config.hpp"
#include "camera_cli.hpp"
#include "v4l2_camera_adapter.hpp"

#include <exception>
#include <filesystem>
#include <iostream>

int main(int argc, char* argv[]) {
    try {
        const std::filesystem::path config_path =
            argc > 1 ? argv[1] : "config/camera_manager.conf";
        const auto config = camera_manager::load_config(config_path);

        camera_manager::V4L2CameraAdapter camera(config.camera_device_index);
        camera_manager::CameraCli cli(camera, config.media_directory);
        cli.run();
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
