#include "camera_agent/agent_config.hpp"

#include "camera_agent/camera_types.hpp"

#include <fstream>
#include <limits>
#include <string>
#include <string_view>

namespace camera_agent {
namespace {

[[nodiscard]] std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

[[nodiscard]] int integer(std::string_view name, const std::string& value) {
    try {
        std::size_t consumed = 0;
        const int result = std::stoi(value, &consumed);
        if (consumed != value.size()) {
            throw std::invalid_argument("trailing characters");
        }
        return result;
    } catch (const std::exception&) {
        throw CameraError(CameraErrorCode::invalid_argument,
                          "Invalid integer for " + std::string(name));
    }
}

[[nodiscard]] std::uint16_t port(std::string_view name,
                                 const std::string& value) {
    const int result = integer(name, value);
    if (result < 1 || result > std::numeric_limits<std::uint16_t>::max()) {
        throw CameraError(CameraErrorCode::invalid_argument,
                          std::string(name) + " must be 1-65535");
    }
    return static_cast<std::uint16_t>(result);
}

}  // namespace

AgentConfig AgentConfig::load(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw CameraError(CameraErrorCode::io_error,
                          "Cannot open config file: " + path.string());
    }

    AgentConfig config;
    bool has_camera_device = false;
    bool has_client_host = false;
    std::string line;
    int line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        line = trim(line.substr(0, line.find('#')));
        if (line.empty()) {
            continue;
        }
        const auto separator = line.find('=');
        if (separator == std::string::npos) {
            throw CameraError(CameraErrorCode::invalid_argument,
                              "Invalid config line " +
                                  std::to_string(line_number));
        }

        const std::string key = trim(line.substr(0, separator));
        const std::string value = trim(line.substr(separator + 1));
        if (key == "camera_device") {
            config.camera.device_path = value;
            has_camera_device = true;
        } else if (key == "client_host") {
            config.client_host = value;
            has_client_host = true;
        } else if (key == "media_directory") {
            config.media_directory = value;
        } else if (key == "width") {
            config.camera.width = integer(key, value);
        } else if (key == "height") {
            config.camera.height = integer(key, value);
        } else if (key == "fps") {
            config.camera.frames_per_second = integer(key, value);
        } else if (key == "jpeg_quality") {
            config.camera.jpeg_quality = integer(key, value);
        } else if (key == "command_port") {
            config.command_port = port(key, value);
        } else if (key == "heartbeat_port") {
            config.heartbeat_port = port(key, value);
        } else if (key == "stream_port") {
            config.stream_port = port(key, value);
        } else if (key == "heartbeat_interval_ms") {
            const int interval = integer(key, value);
            if (interval <= 0) {
                throw CameraError(CameraErrorCode::invalid_argument,
                                  "heartbeat_interval_ms must be positive");
            }
            config.heartbeat_interval = std::chrono::milliseconds(interval);
        } else {
            throw CameraError(CameraErrorCode::invalid_argument,
                              "Unknown config key: " + key);
        }
    }

    if (!has_camera_device || !has_client_host ||
        config.camera.device_path.empty() || config.client_host.empty() ||
        config.media_directory.empty()) {
        throw CameraError(CameraErrorCode::invalid_argument,
                          "camera_device, client_host, and media_directory "
                          "must be configured");
    }
    return config;
}

}  // namespace camera_agent
