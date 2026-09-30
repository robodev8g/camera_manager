#include "app_config.hpp"
#include "cli_command_source.hpp"
#include "command_executor.hpp"
#include "command_handler.hpp"
#include "command_queue.hpp"
#include "tcp_command_source.hpp"
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
        camera_manager::CliCommandSource cli_command_source;
        camera_manager::TcpCommandSource tcp_command_source(
            config.control_port);

        std::exception_ptr cli_error;
        std::thread cli_thread([&] {
            try {
                cli_command_source.run(command_queue);
            } catch (...) {
                cli_error = std::current_exception();
                command_queue.push({
                    {camera_manager::CameraCommandType::shutdown, {}},
                    {},
                });
            }
        });
        std::exception_ptr tcp_error;
        std::thread tcp_thread([&] {
            try {
                tcp_command_source.run(command_queue);
            } catch (...) {
                tcp_error = std::current_exception();
            }
        });
        std::thread command_executor_thread(
            [&] { command_executor.run(); });

        command_executor.run_main_thread();
        tcp_command_source.stop();
        cli_thread.join();
        tcp_thread.join();
        command_executor_thread.join();

        if (cli_error) {
            std::rethrow_exception(cli_error);
        }
        if (tcp_error) {
            std::rethrow_exception(tcp_error);
        }
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
