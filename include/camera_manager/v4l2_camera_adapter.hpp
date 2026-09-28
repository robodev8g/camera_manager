#pragma once

#include "camera_manager/camera_adapter.hpp"

#include <filesystem>
#include <memory>

namespace camera_manager {

class V4L2CameraAdapter final : public CameraAdapter {
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
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace camera_manager
