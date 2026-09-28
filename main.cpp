#include "app_config.hpp"
#include "cli_command_source.hpp"
#include "command_handler.hpp"
#include "v4l2_camera_adapter.hpp"

#include <exception>
#include <filesystem>
#include <iostream>

int main(int argc, char* argv[]) {
    try {
        const std::filesystem::path config_path =
            argc > 1 ? argv[1] : "/home/user/projects/camera_manager/config/camera_manager.json";
        const auto config = camera_manager::load_config(config_path);

        camera_manager::V4L2CameraAdapter camera(config.camera_device_index);
        camera_manager::CommandHandler command_handler(
            camera, config.media_directory);
        camera_manager::CliCommandSource command_source;
        command_handler.run(command_source);
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
