#include "camera_agent/agent_config.hpp"
#include "camera_agent/camera_controller.hpp"
#include "camera_agent/local_cli.hpp"
#include "camera_agent/opencv_camera_backend.hpp"
#include "camera_agent/udp_agent_server.hpp"

#include <csignal>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string_view>
#include <thread>

namespace {

volatile std::sig_atomic_t keep_running = 1;

void stop(int) { keep_running = 0; }

}  // namespace

int main(int argc, char* argv[]) {
    const bool interactive =
        argc == 3 && std::string_view(argv[2]) == "--interactive";
    if (argc != 2 && !interactive) {
        std::cerr << "Usage: camera-agent CONFIG_FILE [--interactive]\n";
        return 2;
    }

    std::signal(SIGINT, stop);
    std::signal(SIGTERM, stop);

    try {
        const auto config =
            camera_agent::AgentConfig::load(std::filesystem::path(argv[1]));
        camera_agent::OpenCvCameraBackend camera(config.camera);
        camera_agent::CameraController controller(config, camera);
        camera_agent::UdpAgentServer server(config, controller);
        std::cout << "Camera agent listening on UDP port "
                  << config.command_port << '\n';
        if (!interactive) {
            server.run(keep_running);
            return 0;
        }

        std::exception_ptr server_error;
        std::jthread server_thread([&] {
            try {
                server.run(keep_running);
            } catch (...) {
                server_error = std::current_exception();
                keep_running = 0;
            }
        });

        try {
            camera_agent::LocalCli cli(controller);
            cli.run(keep_running);
        } catch (...) {
            keep_running = 0;
            server_thread.join();
            throw;
        }
        keep_running = 0;
        server_thread.join();
        if (server_error) {
            std::rethrow_exception(server_error);
        }
    } catch (const std::exception& error) {
        std::cerr << "camera-agent: " << error.what() << '\n';
        return 1;
    }
}
