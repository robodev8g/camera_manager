#pragma once

#include <string>

namespace camera_manager {

enum class CameraCommandType {
    take_snapshot,
    start_recording,
    stop_recording,
    open_local_preview,
    list_media,
    remove_media,
    shutdown,
};

struct CameraCommand {
    CameraCommandType type;
    std::string argument;
};

struct CommandResult {
    std::string message;
    bool success{true};
};

}  // namespace camera_manager
