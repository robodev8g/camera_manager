#include "camera_cli.hpp"

#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace camera_manager {
namespace {

std::filesystem::path make_media_path(const char* type,
                                      const char* extension) {
    const std::filesystem::path media_directory = "media";
    std::filesystem::create_directories(media_directory);

    const std::time_t now = std::time(nullptr);
    const std::tm local_time = *std::localtime(&now);

    std::ostringstream filename;
    filename << type << '_' << std::put_time(&local_time, "%Y%m%d_%H%M%S")
             << extension;
    return media_directory / filename.str();
}

}  // namespace

CameraCli::CameraCli(ICameraAdapter& camera) : camera_(camera) {}

void CameraCli::print_menu() const {
    std::cout << "\nCamera Manager\n"
              << "1. Take snapshot\n"
              << "2. Start recording\n"
              << "3. Stop recording\n"
              << "4. Live view\n"
              << "0. Exit\n"
              << "> ";
}

void CameraCli::run() {
    while (true) {
        print_menu();

        int command = 0;
        std::cin >> command;

        switch (command) {
        case 1: {
            const auto output = make_media_path("photo", ".png");
            camera_.take_snapshot(output);
            std::cout << "Snapshot saved to " << output << '\n';
            break;
        }
        case 2: {
            const auto output = make_media_path("video", ".mp4");
            camera_.start_record(output);
            std::cout << "Recording started: " << output << '\n';
            break;
        }
        case 3:
            camera_.stop_record();
            std::cout << "Recording stopped\n";
            break;
        case 4:
            std::cout << "Press q or Esc to close live view\n";
            camera_.live_view();
            break;
        case 0:
            return;
        }
    }
}

}  // namespace camera_manager
