#pragma once

#include "i_camera_adapter.hpp"

#include <opencv2/core/mat.hpp>
#include <opencv2/videoio.hpp>

#include <atomic>
#include <filesystem>
#include <mutex>
#include <thread>

namespace camera_manager {

class V4L2CameraAdapter final : public ICameraAdapter {
public:
    explicit V4L2CameraAdapter(int device_index = 0);
    ~V4L2CameraAdapter() override;

    V4L2CameraAdapter(const V4L2CameraAdapter&) = delete;
    V4L2CameraAdapter& operator=(const V4L2CameraAdapter&) = delete;
    V4L2CameraAdapter(V4L2CameraAdapter&&) = delete;
    V4L2CameraAdapter& operator=(V4L2CameraAdapter&&) = delete;

    void take_snapshot(const std::filesystem::path& output) override;
    void start_record(const std::filesystem::path& output) override;
    void stop_record() override;
    void live_view() override;

private:
    void capture_loop() noexcept;
    void ensure_running() const;

    cv::VideoCapture camera_;
    cv::VideoWriter recorder_;
    cv::Mat latest_frame_;
    double frames_per_second_{30.0};
    std::mutex mutex_;
    std::thread capture_thread_;
    std::atomic_bool running_{true};
    std::atomic_bool capture_failed_{false};
};

}  // namespace camera_manager
