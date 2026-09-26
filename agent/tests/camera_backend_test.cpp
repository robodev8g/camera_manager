#include "camera_agent/opencv_camera_backend.hpp"
#include "camera_agent/v4l2_controls.hpp"

#include <functional>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void expect_error(camera_agent::CameraErrorCode expected,
                  const std::function<void()>& action,
                  const std::string& description) {
    try {
        action();
        std::cerr << "FAIL: " << description << " did not throw\n";
        ++failures;
    } catch (const camera_agent::CameraError& error) {
        if (error.code() != expected) {
            std::cerr << "FAIL: " << description << " returned wrong error\n";
            ++failures;
        }
    }
}

}  // namespace

int main() {
    using namespace camera_agent;

    expect_error(CameraErrorCode::invalid_argument,
                 [] { V4L2Controls controls(""); }, "empty device path");

    V4L2Controls missing("/dev/camera-manager-missing-device");
    expect_error(CameraErrorCode::invalid_argument,
                 [&] { missing.apply({.zoom = 1.1}); },
                 "invalid normalized zoom");
    expect_error(CameraErrorCode::invalid_argument,
                 [&] { missing.apply({.autofocus = true, .focus = 0.5}); },
                 "autofocus and manual-focus conflict");
    expect_error(CameraErrorCode::device_unavailable,
                 [&] { static_cast<void>(missing.capabilities()); },
                 "missing camera device");

    expect_error(
        CameraErrorCode::invalid_argument,
        [] {
            OpenCvCameraBackend camera({
                .device_path = "/dev/video0",
                .width = 0,
            });
        },
        "invalid capture size");

    if (failures != 0) {
        return 1;
    }
    std::cout << "all tests passed\n";
}

