#pragma once

#include "camera_agent/agent_config.hpp"
#include "camera_agent/camera_backend.hpp"

#include <filesystem>
#include <mutex>
#include <string>

namespace camera_agent {

struct CameraStatus {
    bool recording{false};
    bool network_streaming{false};
    bool local_preview{false};
};

class CameraController final {
public:
    CameraController(AgentConfig config, ICameraBackend& camera);
    ~CameraController();

    CameraController(const CameraController&) = delete;
    CameraController& operator=(const CameraController&) = delete;

    [[nodiscard]] std::filesystem::path take_photo();
    [[nodiscard]] std::filesystem::path start_recording();
    void stop_recording();
    void start_network_stream();
    void stop_network_stream();
    void start_local_preview();
    void stop_local_preview();

    [[nodiscard]] CameraStatus status() const;
    void stop_outputs() noexcept;

private:
    [[nodiscard]] std::filesystem::path media_path(
        const std::string& prefix, const std::string& extension) const;

    AgentConfig config_;
    ICameraBackend& camera_;
    mutable std::mutex mutex_;
    CameraStatus status_;
};

}  // namespace camera_agent
