#include "camera_agent/camera_controller.hpp"

#include "camera_agent/camera_types.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <utility>

namespace camera_agent {

CameraController::CameraController(AgentConfig config, ICameraBackend& camera)
    : config_(std::move(config)), camera_(camera) {
    std::filesystem::create_directories(config_.media_directory);
}

CameraController::~CameraController() { stop_outputs(); }

std::filesystem::path CameraController::take_photo() {
    std::scoped_lock lock(mutex_);
    const auto path = media_path("photo", ".jpg");
    camera_.take_photo(path);
    return path;
}

std::filesystem::path CameraController::start_recording() {
    std::scoped_lock lock(mutex_);
    if (status_.recording) {
        throw CameraError(CameraErrorCode::conflict,
                          "Recording is already active");
    }
    const auto path = media_path("video", ".mp4");
    camera_.start_recording(path);
    status_.recording = true;
    return path;
}

void CameraController::stop_recording() {
    std::scoped_lock lock(mutex_);
    if (!status_.recording) {
        throw CameraError(CameraErrorCode::conflict,
                          "No recording is active");
    }
    camera_.stop_recording();
    status_.recording = false;
}

void CameraController::start_network_stream() {
    std::scoped_lock lock(mutex_);
    if (status_.network_streaming) {
        throw CameraError(CameraErrorCode::conflict,
                          "Network stream is already active");
    }
    camera_.start_live_stream({
        .host = config_.client_host,
        .port = config_.stream_port,
    });
    status_.network_streaming = true;
}

void CameraController::stop_network_stream() {
    std::scoped_lock lock(mutex_);
    if (!status_.network_streaming) {
        throw CameraError(CameraErrorCode::conflict,
                          "No network stream is active");
    }
    camera_.stop_live_stream();
    status_.network_streaming = false;
}

void CameraController::start_local_preview() {
    std::scoped_lock lock(mutex_);
    if (status_.local_preview) {
        throw CameraError(CameraErrorCode::conflict,
                          "Local preview is already active");
    }
    camera_.start_local_preview();
    status_.local_preview = true;
}

void CameraController::stop_local_preview() {
    std::scoped_lock lock(mutex_);
    if (!status_.local_preview) {
        throw CameraError(CameraErrorCode::conflict,
                          "No local preview is active");
    }
    camera_.stop_local_preview();
    status_.local_preview = false;
}

CameraStatus CameraController::status() const {
    std::scoped_lock lock(mutex_);
    return status_;
}

void CameraController::stop_outputs() noexcept {
    std::scoped_lock lock(mutex_);
    if (status_.local_preview) {
        try {
            camera_.stop_local_preview();
        } catch (...) {
        }
        status_.local_preview = false;
    }
    if (status_.recording) {
        try {
            camera_.stop_recording();
        } catch (...) {
        }
        status_.recording = false;
    }
    if (status_.network_streaming) {
        try {
            camera_.stop_live_stream();
        } catch (...) {
        }
        status_.network_streaming = false;
    }
}

std::filesystem::path CameraController::media_path(
    const std::string& prefix, const std::string& extension) const {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  now.time_since_epoch()) %
                              1000;
    std::tm local{};
    localtime_r(&time, &local);

    std::ostringstream name;
    name << prefix << '_' << std::put_time(&local, "%Y%m%d_%H%M%S") << '_'
         << std::setfill('0') << std::setw(3) << milliseconds.count()
         << extension;
    return config_.media_directory / name.str();
}

}  // namespace camera_agent
