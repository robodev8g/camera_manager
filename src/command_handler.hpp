#pragma once

#include "camera_command.hpp"
#include "i_camera_adapter.hpp"

#include <filesystem>

namespace camera_manager {

class CommandHandler {
public:
    CommandHandler(ICameraAdapter& camera,
                   std::filesystem::path media_directory);

    CommandResult handle(const CameraCommand& command);

private:
    std::filesystem::path make_media_path(const char* type,
                                          const char* extension) const;
    CommandResult list_media() const;
    CommandResult remove_media(const std::string& name) const;

    ICameraAdapter& camera_;
    std::filesystem::path media_directory_;
};

}  // namespace camera_manager
