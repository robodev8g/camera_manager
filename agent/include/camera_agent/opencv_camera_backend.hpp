#pragma once

#include "camera_agent/camera_backend.hpp"
#include "camera_agent/v4l2_controls.hpp"

#include <opencv2/core/mat.hpp>
#include <opencv2/videoio.hpp>

#include <atomic>
#include <filesystem>
#include <future>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string>
#include <thread>

namespace camera_agent {

struct OpenCvCameraConfig {
    std::filesystem::path device_path{"/dev/video0"};
    int width{1280};
    int height{720};
    double frames_per_second{30.0};
    int jpeg_quality{90};
};

class OpenCvCameraBackend final : public ICameraBackend {
public:
    explicit OpenCvCameraBackend(OpenCvCameraConfig config);
    ~OpenCvCameraBackend() override;

    OpenCvCameraBackend(const OpenCvCameraBackend&) = delete;
    OpenCvCameraBackend& operator=(const OpenCvCameraBackend&) = delete;
    OpenCvCameraBackend(OpenCvCameraBackend&&) = delete;
    OpenCvCameraBackend& operator=(OpenCvCameraBackend&&) = delete;

    [[nodiscard]] CameraCapabilities capabilities() const override;
    void set_controls(const ControlPatch& controls) override;
    void take_photo(const std::filesystem::path& destination) override;
    void start_recording(const std::filesystem::path& destination) override;
    void stop_recording() override;
    void start_live_stream(const LiveStreamTarget& target) override;
    void stop_live_stream() override;
    void start_local_preview() override;
    void stop_local_preview() override;

private:
    void capture_loop(std::stop_token stop_token);
    void preview_loop(std::stop_token stop_token,
                      const std::shared_ptr<std::promise<void>>& ready) noexcept;
    void stop_local_preview_noexcept() noexcept;
    void ensure_running() const;

    OpenCvCameraConfig config_;
    V4L2Controls controls_;
    CameraCapabilities capabilities_;
    cv::VideoCapture camera_;
    cv::Mat latest_frame_;
    cv::VideoWriter recorder_;
    cv::VideoWriter streamer_;
    mutable std::mutex mutex_;
    std::jthread capture_thread_;
    std::mutex preview_mutex_;
    std::jthread preview_thread_;
    std::atomic_bool capture_failed_{false};
};

}  // namespace camera_agent
