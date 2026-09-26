#include "camera_agent/v4l2_controls.hpp"

#include <linux/videodev2.h>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <fcntl.h>
#include <optional>
#include <string>
#include <string_view>
#include <sys/ioctl.h>
#include <unistd.h>
#include <utility>

namespace camera_agent {
namespace {

class DeviceHandle {
public:
    DeviceHandle(const std::filesystem::path& path, int mode)
        : fd_(::open(path.c_str(), mode | O_CLOEXEC)) {
        if (fd_ < 0) {
            throw CameraError(CameraErrorCode::device_unavailable,
                              "Cannot open " + path.string() + ": " +
                                  std::strerror(errno));
        }
    }

    ~DeviceHandle() { ::close(fd_); }
    DeviceHandle(const DeviceHandle&) = delete;
    DeviceHandle& operator=(const DeviceHandle&) = delete;

    [[nodiscard]] int get() const noexcept { return fd_; }

private:
    int fd_;
};

[[nodiscard]] std::optional<v4l2_queryctrl> query(int fd, std::uint32_t id) {
    v4l2_queryctrl result{};
    result.id = id;
    if (::ioctl(fd, VIDIOC_QUERYCTRL, &result) < 0) {
        if (errno == EINVAL) {
            return std::nullopt;
        }
        throw CameraError(CameraErrorCode::io_error,
                          std::string("Cannot query camera control: ") +
                              std::strerror(errno));
    }
    return (result.flags & V4L2_CTRL_FLAG_DISABLED) == 0U
               ? std::optional(result)
               : std::nullopt;
}

[[nodiscard]] IntegerControlRange range(const v4l2_queryctrl& control) {
    return {
        .minimum = control.minimum,
        .maximum = control.maximum,
        .step = std::max(control.step, 1),
        .default_value = control.default_value,
    };
}

void validate(std::string_view name, double value) {
    if (!std::isfinite(value) || value < 0.0 || value > 1.0) {
        throw CameraError(CameraErrorCode::invalid_argument,
                          std::string(name) + " must be between 0.0 and 1.0");
    }
}

[[nodiscard]] std::int32_t device_value(double normalized,
                                        const v4l2_queryctrl& control) {
    const auto step = std::max(control.step, 1);
    const double raw = control.minimum +
                       normalized * (control.maximum - control.minimum);
    const auto steps = std::llround((raw - control.minimum) / step);
    return static_cast<std::int32_t>(std::clamp(
        static_cast<long long>(control.minimum) + steps * step,
        static_cast<long long>(control.minimum),
        static_cast<long long>(control.maximum)));
}

void set(int fd, std::uint32_t id, std::int32_t value,
         std::string_view name) {
    v4l2_control control{.id = id, .value = value};
    if (::ioctl(fd, VIDIOC_S_CTRL, &control) < 0) {
        throw CameraError(CameraErrorCode::io_error,
                          "Cannot set " + std::string(name) + ": " +
                              std::strerror(errno));
    }
}

}  // namespace

V4L2Controls::V4L2Controls(std::filesystem::path device_path)
    : device_path_(std::move(device_path)) {
    if (device_path_.empty()) {
        throw CameraError(CameraErrorCode::invalid_argument,
                          "Camera device path must not be empty");
    }
}

CameraControlCapabilities V4L2Controls::capabilities() const {
    const DeviceHandle device(device_path_, O_RDONLY);
    CameraControlCapabilities result;
    if (const auto zoom = query(device.get(), V4L2_CID_ZOOM_ABSOLUTE)) {
        result.zoom = range(*zoom);
    }
    if (const auto focus = query(device.get(), V4L2_CID_FOCUS_ABSOLUTE)) {
        result.focus = range(*focus);
    }
    result.autofocus = query(device.get(), V4L2_CID_FOCUS_AUTO).has_value();
    return result;
}

void V4L2Controls::apply(const ControlPatch& controls) const {
    if (controls.zoom) {
        validate("zoom", *controls.zoom);
    }
    if (controls.focus) {
        validate("focus", *controls.focus);
    }
    if (controls.autofocus.value_or(false) && controls.focus) {
        throw CameraError(CameraErrorCode::invalid_argument,
                          "Cannot set manual focus while enabling autofocus");
    }

    const DeviceHandle device(device_path_, O_RDWR);
    const auto zoom = controls.zoom
                          ? query(device.get(), V4L2_CID_ZOOM_ABSOLUTE)
                          : std::nullopt;
    const auto autofocus = (controls.autofocus || controls.focus)
                               ? query(device.get(), V4L2_CID_FOCUS_AUTO)
                               : std::nullopt;
    const auto focus = controls.focus
                           ? query(device.get(), V4L2_CID_FOCUS_ABSOLUTE)
                           : std::nullopt;

    if (controls.zoom && !zoom) {
        throw CameraError(CameraErrorCode::unsupported,
                          "Camera does not support zoom");
    }
    if (controls.autofocus && !autofocus) {
        throw CameraError(CameraErrorCode::unsupported,
                          "Camera does not support autofocus");
    }
    if (controls.focus && !focus) {
        throw CameraError(CameraErrorCode::unsupported,
                          "Camera does not support manual focus");
    }

    if (controls.zoom) {
        set(device.get(), V4L2_CID_ZOOM_ABSOLUTE,
            device_value(*controls.zoom, *zoom), "zoom");
    }
    if (controls.autofocus) {
        set(device.get(), V4L2_CID_FOCUS_AUTO, *controls.autofocus ? 1 : 0,
            "autofocus");
    } else if (controls.focus && autofocus) {
        set(device.get(), V4L2_CID_FOCUS_AUTO, 0, "autofocus");
    }
    if (controls.focus) {
        set(device.get(), V4L2_CID_FOCUS_ABSOLUTE,
            device_value(*controls.focus, *focus), "focus");
    }
}

}  // namespace camera_agent

