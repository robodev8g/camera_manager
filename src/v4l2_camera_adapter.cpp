#include "v4l2_camera_adapter.hpp"

#include <arpa/inet.h>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>

#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace camera_manager {

V4L2CameraAdapter::V4L2CameraAdapter(int device_index)
    : camera_(device_index, cv::CAP_V4L2) {
    if (!camera_.isOpened()) {
        throw std::runtime_error("could not open the V4L2 camera");
    }

    if (!camera_.read(latest_frame_) || latest_frame_.empty()) {
        throw std::runtime_error("the camera did not produce a frame");
    }

    frames_per_second_ = camera_.get(cv::CAP_PROP_FPS);
    if (frames_per_second_ <= 0.0) {
        frames_per_second_ = 30.0;
    }

    capture_thread_ = std::thread([this] { capture_loop(); });
}

V4L2CameraAdapter::~V4L2CameraAdapter() {
    running_ = false;
    if (capture_thread_.joinable()) {
        capture_thread_.join();
    }

    std::lock_guard<std::mutex> lock(mutex_);
    recorder_.release();
    stream_writer_.release();
    camera_.release();
}

void V4L2CameraAdapter::capture_loop() noexcept {
    try {
        cv::Mat frame;
        while (running_) {
            if (!camera_.read(frame) || frame.empty()) {
                capture_failed_ = true;
                return;
            }

            std::lock_guard<std::mutex> lock(mutex_);
            frame.copyTo(latest_frame_);
            if (recorder_.isOpened()) {
                recorder_.write(frame);
            }
            if (stream_writer_.isOpened()) {
                stream_writer_.write(frame);
            }
        }
    } catch (...) {
        capture_failed_ = true;
    }
}

void V4L2CameraAdapter::ensure_running() const {
    if (capture_failed_) {
        throw std::runtime_error("camera capture stopped");
    }
}

void V4L2CameraAdapter::take_snapshot(
    const std::filesystem::path& output) {
    if (output.empty()) {
        throw std::invalid_argument("snapshot path must not be empty");
    }

    cv::Mat snapshot;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ensure_running();
        latest_frame_.copyTo(snapshot);
    }

    if (!cv::imwrite(output.string(), snapshot)) {
        throw std::runtime_error("could not save snapshot: " +
                                 output.string());
    }
}

void V4L2CameraAdapter::start_record(
    const std::filesystem::path& output) {
    if (output.empty()) {
        throw std::invalid_argument("recording path must not be empty");
    }

    std::lock_guard<std::mutex> lock(mutex_);
    ensure_running();
    if (recorder_.isOpened()) {
        throw std::logic_error("a recording is already active");
    }

    const int codec = cv::VideoWriter::fourcc('m', 'p', '4', 'v');
    const cv::Size size(latest_frame_.cols, latest_frame_.rows);
    if (!recorder_.open(output.string(), codec, frames_per_second_, size)) {
        throw std::runtime_error("could not start recording: " +
                                 output.string());
    }
}

void V4L2CameraAdapter::stop_record() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!recorder_.isOpened()) {
        throw std::logic_error("no recording is active");
    }
    recorder_.release();
}

void V4L2CameraAdapter::start_stream(
    const std::string& destination,
    std::uint16_t port) {
    in_addr address{};
    if (::inet_pton(AF_INET, destination.c_str(), &address) != 1) {
        throw std::invalid_argument(
            "stream destination must be an IPv4 address");
    }
    if (port == 0) {
        throw std::invalid_argument("stream port must not be zero");
    }

    std::lock_guard<std::mutex> lock(mutex_);
    ensure_running();
    if (stream_writer_.isOpened()) {
        throw std::logic_error("a stream is already active");
    }

    const int gop_size =
        std::max(1, static_cast<int>(frames_per_second_));
    std::ostringstream pipeline;
    pipeline
        << "appsrc ! queue max-size-buffers=1 leaky=downstream ! "
        << "videoconvert ! video/x-raw,format=I420 ! "
        << "openh264enc complexity=low rate-control=bitrate "
        << "bitrate=2000000 gop-size=" << gop_size << " ! "
        << "rtph264pay config-interval=1 pt=96 ! "
        << "udpsink host=" << destination << " port=" << port
        << " sync=false async=false";

    const cv::Size size(latest_frame_.cols, latest_frame_.rows);
    if (!stream_writer_.open(
            pipeline.str(), cv::CAP_GSTREAMER, 0,
            frames_per_second_, size, true)) {
        throw std::runtime_error("could not start the RTP video stream");
    }
}

void V4L2CameraAdapter::stop_stream() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!stream_writer_.isOpened()) {
        throw std::logic_error("no stream is active");
    }
    stream_writer_.release();
}

void V4L2CameraAdapter::live_view(
    const std::atomic_bool& stop_requested) {
    constexpr const char* window_name = "Camera Manager";
    cv::namedWindow(window_name, cv::WINDOW_AUTOSIZE);

    try {
        while (!stop_requested.load()) {
            cv::Mat frame;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                ensure_running();
                latest_frame_.copyTo(frame);
            }

            cv::imshow(window_name, frame);
            const int key = cv::waitKey(1);
            if (key == 'q' || key == 27 ||
                cv::getWindowProperty(window_name, cv::WND_PROP_VISIBLE) < 1) {
                break;
            }
        }
    } catch (...) {
        cv::destroyWindow(window_name);
        throw;
    }

    cv::destroyWindow(window_name);
}

}  // namespace camera_manager
