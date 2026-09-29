#include "app_config.hpp"
#include "cli_command_source.hpp"
#include "command_executor.hpp"
#include "command_handler.hpp"
#include "command_queue.hpp"
#include "v4l2_camera_adapter.hpp"

#include <exception>
#include <filesystem>
#include <iostream>
#include <thread>

int main(int argc, char* argv[]) {
    try {
        const std::filesystem::path config_path =
            argc > 1 ? argv[1] : "/home/user/projects/camera_manager/config/camera_manager.json";
        const auto config = camera_manager::load_config(config_path);

        camera_manager::V4L2CameraAdapter camera(config.camera_device_index);
        camera_manager::CommandQueue command_queue;
        camera_manager::CommandHandler command_handler(
            camera, config.media_directory);
        camera_manager::CommandExecutor command_executor(
            command_handler, command_queue);
        camera_manager::CliCommandSource command_source;

        std::exception_ptr command_source_error;
        std::thread command_source_thread([&] {
            try {
                command_source.run(command_queue);
            } catch (...) {
                command_source_error = std::current_exception();
                command_queue.push({
                    {camera_manager::CameraCommandType::shutdown, {}},
                    {},
                });
            }
        });
        std::thread command_executor_thread(
            [&] { command_executor.run(); });

        command_executor.run_main_thread();
        command_source_thread.join();
        command_executor_thread.join();

        if (command_source_error) {
            std::rethrow_exception(command_source_error);
        }
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
