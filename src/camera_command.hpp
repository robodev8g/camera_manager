#pragma once

#include <cstdint>
#include <string>

namespace camera_manager {

enum class CameraCommandType {
    take_snapshot,
    start_recording,
    stop_recording,
    start_stream,
    stop_stream,
    open_local_preview,
    list_media,
    remove_media,
    shutdown,
};

struct CameraCommand {
    CameraCommandType type;
    std::string argument;
    std::uint16_t port{0};
};

struct CommandResult {
    std::string message;
    bool success{true};
};

}  // namespace camera_manager
