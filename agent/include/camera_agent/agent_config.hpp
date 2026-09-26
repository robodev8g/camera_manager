#pragma once

#include "camera_agent/opencv_camera_backend.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>

namespace camera_agent {

struct AgentConfig {
    OpenCvCameraConfig camera;
    std::string client_host;
    std::filesystem::path media_directory{"media"};
    std::uint16_t command_port{7000};
    std::uint16_t heartbeat_port{7001};
    std::uint16_t stream_port{5000};
    std::chrono::milliseconds heartbeat_interval{1000};

    [[nodiscard]] static AgentConfig load(const std::filesystem::path& path);
};

}  // namespace camera_agent

