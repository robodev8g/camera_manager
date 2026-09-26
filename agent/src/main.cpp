#include "camera_agent/agent_config.hpp"
#include "camera_agent/opencv_camera_backend.hpp"
#include "camera_agent/udp_agent_server.hpp"

#include <csignal>
#include <filesystem>
#include <iostream>

namespace {

volatile std::sig_atomic_t keep_running = 1;

void stop(int) { keep_running = 0; }

}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: camera-agent CONFIG_FILE\n";
        return 2;
    }

    std::signal(SIGINT, stop);
    std::signal(SIGTERM, stop);

    try {
        const auto config =
            camera_agent::AgentConfig::load(std::filesystem::path(argv[1]));
        camera_agent::OpenCvCameraBackend camera(config.camera);
        camera_agent::UdpAgentServer server(config, camera);
        std::cout << "Camera agent listening on UDP port "
                  << config.command_port << '\n';
        server.run(keep_running);
    } catch (const std::exception& error) {
        std::cerr << "camera-agent: " << error.what() << '\n';
        return 1;
    }
}

