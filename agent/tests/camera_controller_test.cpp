#include "camera_agent/camera_controller.hpp"

#include <filesystem>
#include <functional>
#include <iostream>
#include <string>

namespace {

class FakeCameraBackend final : public camera_agent::ICameraBackend {
public:
    [[nodiscard]] camera_agent::CameraCapabilities capabilities() const override {
        return {};
    }

    void set_controls(const camera_agent::ControlPatch&) override {}

    void take_photo(const std::filesystem::path& destination) override {
        ++photo_count;
        last_photo = destination;
    }

    void start_recording(const std::filesystem::path& destination) override {
        ++recording_start_count;
        last_recording = destination;
    }

    void stop_recording() override { ++recording_stop_count; }

    void start_live_stream(
        const camera_agent::LiveStreamTarget& target) override {
        ++network_start_count;
        last_target = target;
    }

    void stop_live_stream() override { ++network_stop_count; }

    void start_local_preview() override { ++preview_start_count; }

    void stop_local_preview() override { ++preview_stop_count; }

    int photo_count{};
    int recording_start_count{};
    int recording_stop_count{};
    int network_start_count{};
    int network_stop_count{};
    int preview_start_count{};
    int preview_stop_count{};
    std::filesystem::path last_photo;
    std::filesystem::path last_recording;
    camera_agent::LiveStreamTarget last_target;
};

int failures = 0;

void expect(bool condition, const std::string& description) {
    if (!condition) {
        std::cerr << "FAIL: " << description << '\n';
        ++failures;
    }
}

void expect_conflict(const std::function<void()>& action,
                     const std::string& description) {
    try {
        action();
        std::cerr << "FAIL: " << description << " did not throw\n";
        ++failures;
    } catch (const camera_agent::CameraError& error) {
        if (error.code() != camera_agent::CameraErrorCode::conflict) {
            std::cerr << "FAIL: " << description << " returned wrong error\n";
            ++failures;
        }
    }
}

}  // namespace

int main() {
    camera_agent::AgentConfig config;
    config.client_host = "192.0.2.10";
    config.stream_port = 4321;
    config.media_directory = std::filesystem::temp_directory_path();

    FakeCameraBackend camera;
    camera_agent::CameraController controller(config, camera);

    const auto photo = controller.take_photo();
    expect(camera.photo_count == 1, "photo reaches backend");
    expect(photo.extension() == ".jpg", "photo uses JPEG extension");
    expect(photo == camera.last_photo, "photo path is returned");

    const auto recording = controller.start_recording();
    expect(recording.extension() == ".mp4", "recording uses MP4 extension");
    expect(recording == camera.last_recording, "recording path is returned");
    expect_conflict([&] { static_cast<void>(controller.start_recording()); },
                    "duplicate recording start");

    controller.start_network_stream();
    controller.start_local_preview();
    const auto active = controller.status();
    expect(active.recording, "recording status is active");
    expect(active.network_streaming, "network status is active");
    expect(active.local_preview, "preview status is active");
    expect(camera.last_target.host == config.client_host,
           "network stream uses configured host");
    expect(camera.last_target.port == config.stream_port,
           "network stream uses configured port");

    controller.stop_recording();
    controller.stop_network_stream();
    controller.stop_local_preview();
    const auto stopped = controller.status();
    expect(!stopped.recording && !stopped.network_streaming &&
               !stopped.local_preview,
           "all statuses stop independently");

    expect_conflict([&] { controller.stop_recording(); },
                    "recording stop while inactive");
    expect_conflict([&] { controller.stop_network_stream(); },
                    "network stop while inactive");
    expect_conflict([&] { controller.stop_local_preview(); },
                    "preview stop while inactive");

    static_cast<void>(controller.start_recording());
    controller.start_network_stream();
    controller.start_local_preview();
    controller.stop_outputs();
    controller.stop_outputs();
    expect(camera.recording_stop_count == 2,
           "shutdown stops each recording once");
    expect(camera.network_stop_count == 2,
           "shutdown stops each network stream once");
    expect(camera.preview_stop_count == 2,
           "shutdown stops each preview once");

    if (failures != 0) {
        return 1;
    }
    std::cout << "all controller tests passed\n";
}
