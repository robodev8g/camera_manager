#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace camera_agent {

enum class CameraErrorCode {
    invalid_argument,
    device_unavailable,
    unsupported,
    conflict,
    io_error,
};

class CameraError final : public std::runtime_error {
public:
    CameraError(CameraErrorCode code, std::string message)
        : std::runtime_error(std::move(message)), code_(code) {}

    [[nodiscard]] CameraErrorCode code() const noexcept { return code_; }

private:
    CameraErrorCode code_;
};

struct IntegerControlRange {
    std::int32_t minimum{};
    std::int32_t maximum{};
    std::int32_t step{1};
    std::int32_t default_value{};
};

struct CameraControlCapabilities {
    std::optional<IntegerControlRange> zoom;
    std::optional<IntegerControlRange> focus;
    bool autofocus{false};
};

struct CameraCapabilities {
    std::string device_path;
    int width{};
    int height{};
    double frames_per_second{};
    CameraControlCapabilities controls;
};

// Zoom and focus positions use normalized [0.0, 1.0] values. The V4L2
// adapter translates them to the integer range reported by the device.
struct ControlPatch {
    std::optional<double> zoom;
    std::optional<bool> autofocus;
    std::optional<double> focus;
};

struct LiveStreamTarget {
    std::string host;
    std::uint16_t port{};
    std::uint32_t bitrate_bits_per_second{1'500'000};
};

}  // namespace camera_agent

