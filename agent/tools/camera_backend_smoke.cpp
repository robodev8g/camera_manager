#include "camera_agent/opencv_camera_backend.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: camera-backend-smoke DEVICE "
                     "[--photo FILE.jpg] [--record FILE.mp4 SECONDS] "
                     "[--stream CLIENT_IP PORT SECONDS]\n";
        return 2;
    }

    try {
        camera_agent::OpenCvCameraBackend camera({
            .device_path = std::filesystem::path(argv[1]),
        });
        const auto capabilities = camera.capabilities();
        std::cout << capabilities.device_path << ": " << capabilities.width
                  << 'x' << capabilities.height << " @ "
                  << capabilities.frames_per_second << " fps\n"
                  << "zoom: " << (capabilities.controls.zoom ? "yes" : "no")
                  << ", focus: "
                  << (capabilities.controls.focus ? "yes" : "no")
                  << ", autofocus: "
                  << (capabilities.controls.autofocus ? "yes" : "no") << '\n';

        for (int index = 2; index < argc;) {
            const std::string option = argv[index++];
            if (option == "--photo" && index < argc) {
                camera.take_photo(argv[index]);
                std::cout << "photo: " << argv[index++] << '\n';
            } else if (option == "--record" && index + 1 < argc) {
                const std::filesystem::path output(argv[index++]);
                const int seconds = std::stoi(argv[index++]);
                if (seconds <= 0) {
                    throw std::invalid_argument(
                        "Recording duration must be positive");
                }
                camera.start_recording(output);
                std::this_thread::sleep_for(std::chrono::seconds(seconds));
                camera.stop_recording();
                std::cout << "recording: " << output << '\n';
            } else if (option == "--stream" && index + 2 < argc) {
                const std::string host(argv[index++]);
                const auto raw_port = std::stoul(argv[index++]);
                const int seconds = std::stoi(argv[index++]);
                if (raw_port > 65'535UL || seconds <= 0) {
                    throw std::invalid_argument(
                        "Stream port and duration are invalid");
                }
                const auto port = static_cast<std::uint16_t>(raw_port);
                camera.start_live_stream({.host = host, .port = port});
                std::this_thread::sleep_for(std::chrono::seconds(seconds));
                camera.stop_live_stream();
                std::cout << "streamed to " << host << ':' << port << '\n';
            } else {
                throw std::invalid_argument("Invalid smoke-test arguments");
            }
        }
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}

