#pragma once

#include "camera_agent/camera_types.hpp"

#include <filesystem>

namespace camera_agent {

class ICameraBackend {
public:
    virtual ~ICameraBackend() = default;

    [[nodiscard]] virtual CameraCapabilities capabilities() const = 0;
    virtual void set_controls(const ControlPatch& controls) = 0;
    virtual void take_photo(const std::filesystem::path& destination) = 0;
    virtual void start_recording(const std::filesystem::path& destination) = 0;
    virtual void stop_recording() = 0;
    virtual void start_live_stream(const LiveStreamTarget& target) = 0;
    virtual void stop_live_stream() = 0;
    virtual void start_local_preview() = 0;
    virtual void stop_local_preview() = 0;
};

}  // namespace camera_agent
