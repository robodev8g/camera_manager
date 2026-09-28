#pragma once

#include <filesystem>

namespace camera_manager {

struct AppConfig {
    int camera_device_index;
    std::filesystem::path media_directory;
};

AppConfig load_config(const std::filesystem::path& config_path);

}  // namespace camera_manager
