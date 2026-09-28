#include "v4l2_camera_adapter.hpp"

#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/videoio.hpp>

#include <atomic>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>

namespace camera_manager {

struct V4L2CameraAdapter::Impl {
    explicit Impl(int device_index) : camera(device_index, cv::CAP_V4L2) {
        if (!camera.isOpened()) {
            throw std::runtime_error("could not open the V4L2 camera");
        }

        if (!camera.read(latest_frame) || latest_frame.empty()) {
            throw std::runtime_error("the camera did not produce a frame");
        }

        frames_per_second = camera.get(cv::CAP_PROP_FPS);
        if (frames_per_second <= 0.0) {
            frames_per_second = 30.0;
        }

        capture_thread = std::thread([this] { capture_loop(); });
    }

    ~Impl() {
        running = false;
        if (capture_thread.joinable()) {
            capture_thread.join();
        }

        std::lock_guard<std::mutex> lock(mutex);
        recorder.release();
        camera.release();
    }

    void ensure_running() const {
        if (capture_failed) {
            throw std::runtime_error("camera capture stopped");
        }
    }

    void capture_loop() noexcept {
        try {
            cv::Mat frame;
            while (running) {
                if (!camera.read(frame) || frame.empty()) {
                    capture_failed = true;
                    return;
                }

                std::lock_guard<std::mutex> lock(mutex);
                frame.copyTo(latest_frame);
                if (recorder.isOpened()) {
                    recorder.write(frame);
                }
            }
        } catch (...) {
            capture_failed = true;
        }
    }

    cv::VideoCapture camera;
    cv::VideoWriter recorder;
    cv::Mat latest_frame;
    double frames_per_second{30.0};
    std::mutex mutex;
    std::thread capture_thread;
    std::atomic_bool running{true};
    std::atomic_bool capture_failed{false};
};

V4L2CameraAdapter::V4L2CameraAdapter(int device_index)
    : impl_(std::make_unique<Impl>(device_index)) {}

V4L2CameraAdapter::~V4L2CameraAdapter() = default;

void V4L2CameraAdapter::take_snapshot(
    const std::filesystem::path& output) {
    if (output.empty()) {
        throw std::invalid_argument("snapshot path must not be empty");
    }

    cv::Mat snapshot;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->ensure_running();
        impl_->latest_frame.copyTo(snapshot);
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

    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->ensure_running();
    if (impl_->recorder.isOpened()) {
        throw std::logic_error("a recording is already active");
    }

    const int codec = cv::VideoWriter::fourcc('m', 'p', '4', 'v');
    const cv::Size size(impl_->latest_frame.cols, impl_->latest_frame.rows);
    if (!impl_->recorder.open(output.string(), codec,
                              impl_->frames_per_second, size)) {
        throw std::runtime_error("could not start recording: " +
                                 output.string());
    }
}

void V4L2CameraAdapter::stop_record() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->recorder.isOpened()) {
        throw std::logic_error("no recording is active");
    }
    impl_->recorder.release();
}

void V4L2CameraAdapter::live_view() {
    constexpr const char* window_name = "Camera Manager";
    cv::namedWindow(window_name, cv::WINDOW_AUTOSIZE);

    try {
        while (true) {
            cv::Mat frame;
            {
                std::lock_guard<std::mutex> lock(impl_->mutex);
                impl_->ensure_running();
                impl_->latest_frame.copyTo(frame);
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
