#pragma once

#include <cstdint>
#include <filesystem>

namespace camera_manager {

struct AppConfig {
    int camera_device_index;
    std::filesystem::path media_directory;
    std::uint16_t control_port;
};

AppConfig load_config(const std::filesystem::path& config_path);

}  // namespace camera_manager
