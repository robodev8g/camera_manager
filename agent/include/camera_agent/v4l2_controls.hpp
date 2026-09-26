#pragma once

#include "camera_agent/camera_types.hpp"

#include <filesystem>

namespace camera_agent {

class V4L2Controls final {
public:
    explicit V4L2Controls(std::filesystem::path device_path);

    [[nodiscard]] CameraControlCapabilities capabilities() const;
    void apply(const ControlPatch& controls) const;

private:
    std::filesystem::path device_path_;
};

}  // namespace camera_agent

