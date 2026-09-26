#include "camera_agent/opencv_camera_backend.hpp"

#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>

#include <arpa/inet.h>

#include <chrono>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <future>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace camera_agent {
namespace {

void validate_destination(const std::filesystem::path& destination) {
    if (destination.empty()) {
        throw CameraError(CameraErrorCode::invalid_argument,
                          "Destination path must not be empty");
    }
    std::error_code error;
    if (std::filesystem::exists(destination, error)) {
        throw CameraError(CameraErrorCode::conflict,
                          "Destination already exists: " + destination.string());
    }
    const auto parent = destination.parent_path();
    if (error || (!parent.empty() &&
                  !std::filesystem::is_directory(parent, error))) {
        throw CameraError(CameraErrorCode::io_error,
                          "Destination directory is unavailable");
    }
}

void validate_host(const std::string& host) {
    in_addr ipv4{};
    in6_addr ipv6{};
    if (inet_pton(AF_INET, host.c_str(), &ipv4) != 1 &&
        inet_pton(AF_INET6, host.c_str(), &ipv6) != 1) {
        throw CameraError(CameraErrorCode::invalid_argument,
                          "Live stream host must be an IPv4 or IPv6 address");
    }
}

[[nodiscard]] std::string stream_pipeline(const LiveStreamTarget& target) {
    std::ostringstream pipeline;
    pipeline << "appsrc is-live=true format=time ! "
             << "queue leaky=downstream max-size-buffers=2 ! "
             << "videoconvert ! video/x-raw,format=I420 ! "
             << "openh264enc bitrate=" << target.bitrate_bits_per_second << " ! "
             << "rtph264pay config-interval=1 pt=96 ! "
             << "udpsink host=" << target.host << " port=" << target.port
             << " sync=false async=false";
    return pipeline.str();
}

}  // namespace

OpenCvCameraBackend::OpenCvCameraBackend(OpenCvCameraConfig config)
    : config_(std::move(config)), controls_(config_.device_path) {
    if (config_.width <= 0 || config_.height <= 0 ||
        config_.frames_per_second <= 0.0) {
        throw CameraError(CameraErrorCode::invalid_argument,
                          "Width, height, and frame rate must be positive");
    }
    if (config_.jpeg_quality < 0 || config_.jpeg_quality > 100) {
        throw CameraError(CameraErrorCode::invalid_argument,
                          "JPEG quality must be between 0 and 100");
    }

    if (!camera_.open(config_.device_path.string(), cv::CAP_V4L2)) {
        throw CameraError(CameraErrorCode::device_unavailable,
                          "Cannot open camera: " + config_.device_path.string());
    }
    camera_.set(cv::CAP_PROP_FRAME_WIDTH, config_.width);
    camera_.set(cv::CAP_PROP_FRAME_HEIGHT, config_.height);
    camera_.set(cv::CAP_PROP_FPS, config_.frames_per_second);
    camera_.set(cv::CAP_PROP_BUFFERSIZE, 1);

    if (!camera_.read(latest_frame_) || latest_frame_.empty()) {
        throw CameraError(CameraErrorCode::device_unavailable,
                          "Camera did not produce an initial frame");
    }

    capabilities_ = {
        .device_path = config_.device_path.string(),
        .width = latest_frame_.cols,
        .height = latest_frame_.rows,
        .frames_per_second = camera_.get(cv::CAP_PROP_FPS),
        .controls = controls_.capabilities(),
    };
    if (capabilities_.frames_per_second <= 0.0) {
        capabilities_.frames_per_second = config_.frames_per_second;
    }

    capture_thread_ =
        std::jthread([this](std::stop_token token) { capture_loop(token); });
}

OpenCvCameraBackend::~OpenCvCameraBackend() {
    stop_local_preview_noexcept();
    capture_thread_.request_stop();
    if (capture_thread_.joinable()) {
        capture_thread_.join();
    }
    std::scoped_lock lock(mutex_);
    recorder_.release();
    streamer_.release();
    camera_.release();
}

CameraCapabilities OpenCvCameraBackend::capabilities() const {
    return capabilities_;
}

void OpenCvCameraBackend::set_controls(const ControlPatch& controls) {
    ensure_running();
    controls_.apply(controls);
}

void OpenCvCameraBackend::take_photo(
    const std::filesystem::path& destination) {
    validate_destination(destination);
    cv::Mat photo;
    {
        std::scoped_lock lock(mutex_);
        ensure_running();
        latest_frame_.copyTo(photo);
    }
    const std::vector<int> parameters{cv::IMWRITE_JPEG_QUALITY,
                                      config_.jpeg_quality};
    if (!cv::imwrite(destination.string(), photo, parameters)) {
        throw CameraError(CameraErrorCode::io_error,
                          "Cannot write photo: " + destination.string());
    }
}

void OpenCvCameraBackend::start_recording(
    const std::filesystem::path& destination) {
    validate_destination(destination);
    std::scoped_lock lock(mutex_);
    ensure_running();
    if (recorder_.isOpened()) {
        throw CameraError(CameraErrorCode::conflict,
                          "A recording is already active");
    }
    const auto codec = cv::VideoWriter::fourcc('m', 'p', '4', 'v');
    if (!recorder_.open(destination.string(), codec,
                        capabilities_.frames_per_second,
                        {capabilities_.width, capabilities_.height})) {
        throw CameraError(CameraErrorCode::io_error,
                          "Cannot start recording: " + destination.string());
    }
}

void OpenCvCameraBackend::stop_recording() {
    std::scoped_lock lock(mutex_);
    if (!recorder_.isOpened()) {
        throw CameraError(CameraErrorCode::conflict,
                          "No recording is active");
    }
    recorder_.release();
}

void OpenCvCameraBackend::start_live_stream(const LiveStreamTarget& target) {
    validate_host(target.host);
    if (target.port == 0 || target.bitrate_bits_per_second == 0) {
        throw CameraError(CameraErrorCode::invalid_argument,
                          "Stream port and bitrate must be positive");
    }

    std::scoped_lock lock(mutex_);
    ensure_running();
    if (streamer_.isOpened()) {
        throw CameraError(CameraErrorCode::conflict,
                          "A live stream is already active");
    }
    if (!streamer_.open(stream_pipeline(target), cv::CAP_GSTREAMER, 0,
                        capabilities_.frames_per_second,
                        {capabilities_.width, capabilities_.height})) {
        throw CameraError(CameraErrorCode::io_error,
                          "Cannot start the GStreamer live stream");
    }
}

void OpenCvCameraBackend::stop_live_stream() {
    std::scoped_lock lock(mutex_);
    if (!streamer_.isOpened()) {
        throw CameraError(CameraErrorCode::conflict,
                          "No live stream is active");
    }
    streamer_.release();
}

void OpenCvCameraBackend::start_local_preview() {
    std::scoped_lock preview_lock(preview_mutex_);
    ensure_running();
    if (preview_thread_.joinable()) {
        throw CameraError(CameraErrorCode::conflict,
                          "A local preview is already active");
    }

#if defined(__linux__)
    const char* display = std::getenv("DISPLAY");
    const char* wayland_display = std::getenv("WAYLAND_DISPLAY");
    if ((display == nullptr || display[0] == '\0') &&
        (wayland_display == nullptr || wayland_display[0] == '\0')) {
        throw CameraError(CameraErrorCode::device_unavailable,
                          "Local preview requires a graphical display");
    }
#endif

    auto ready = std::make_shared<std::promise<void>>();
    auto started = ready->get_future();
    preview_thread_ = std::jthread(
        [this, ready](std::stop_token token) { preview_loop(token, ready); });
    try {
        started.get();
    } catch (const std::exception& error) {
        preview_thread_.request_stop();
        preview_thread_.join();
        throw CameraError(CameraErrorCode::io_error,
                          "Cannot start local preview: " +
                              std::string(error.what()));
    }
}

void OpenCvCameraBackend::stop_local_preview() {
    std::scoped_lock preview_lock(preview_mutex_);
    if (!preview_thread_.joinable()) {
        throw CameraError(CameraErrorCode::conflict,
                          "No local preview is active");
    }
    preview_thread_.request_stop();
    preview_thread_.join();
}

void OpenCvCameraBackend::preview_loop(
    std::stop_token stop_token,
    const std::shared_ptr<std::promise<void>>& ready) noexcept {
    constexpr const char* window_name = "Camera Agent Preview";
    bool window_created = false;
    bool startup_reported = false;
    try {
        cv::namedWindow(window_name, cv::WINDOW_AUTOSIZE);
        window_created = true;
        ready->set_value();
        startup_reported = true;

        while (!stop_token.stop_requested() && !capture_failed_.load()) {
            cv::Mat frame;
            {
                std::scoped_lock lock(mutex_);
                latest_frame_.copyTo(frame);
            }
            if (!frame.empty()) {
                cv::imshow(window_name, frame);
            }
            static_cast<void>(cv::waitKey(15));
        }
    } catch (...) {
        if (!startup_reported) {
            try {
                ready->set_exception(std::current_exception());
                startup_reported = true;
            } catch (...) {
            }
        }
    }

    if (window_created) {
        try {
            cv::destroyWindow(window_name);
            static_cast<void>(cv::waitKey(1));
        } catch (...) {
        }
    }
}

void OpenCvCameraBackend::stop_local_preview_noexcept() noexcept {
    try {
        std::scoped_lock preview_lock(preview_mutex_);
        if (preview_thread_.joinable()) {
            preview_thread_.request_stop();
            preview_thread_.join();
        }
    } catch (...) {
    }
}

void OpenCvCameraBackend::capture_loop(std::stop_token stop_token) {
    cv::Mat frame;
    try {
        while (!stop_token.stop_requested()) {
            if (!camera_.read(frame) || frame.empty()) {
                capture_failed_ = true;
                return;
            }

            std::scoped_lock lock(mutex_);
            frame.copyTo(latest_frame_);
            if (recorder_.isOpened()) {
                recorder_.write(frame);
            }
            if (streamer_.isOpened()) {
                streamer_.write(frame);
            }
        }
    } catch (const cv::Exception&) {
        capture_failed_ = true;
    }
}

void OpenCvCameraBackend::ensure_running() const {
    if (capture_failed_.load()) {
        throw CameraError(CameraErrorCode::device_unavailable,
                          "Camera capture has stopped");
    }
}

}  // namespace camera_agent
