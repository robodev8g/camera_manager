#pragma once

#include "i_camera_adapter.hpp"

#include <filesystem>

namespace camera_manager {

class CameraCli {
public:
    CameraCli(ICameraAdapter& camera, std::filesystem::path media_dir_path);

    void run();

private:
    void print_menu() const;

    ICameraAdapter& camera_;
    std::filesystem::path media_dir_path_;
};

}  // namespace camera_manager
