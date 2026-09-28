#include "cli_command_source.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace camera_manager {

void CliCommandSource::print_menu() const {
    std::cout << "\nCamera Manager\n"
              << "1. Take snapshot\n"
              << "2. Start recording\n"
              << "3. Stop recording\n"
              << "4. Live view\n"
              << "5. List media\n"
              << "6. Remove media\n"
              << "0. Exit\n"
              << "> ";
}

CameraCommand CliCommandSource::wait_for_command() {
    print_menu();

    int command = 0;
    std::cin >> command;

    switch (command) {
    case 1:
        return {CameraCommandType::take_snapshot, {}};
    case 2:
        return {CameraCommandType::start_recording, {}};
    case 3:
        return {CameraCommandType::stop_recording, {}};
    case 4:
        return {CameraCommandType::open_local_preview, {}};
    case 5:
        return {CameraCommandType::list_media, {}};
    case 6: {
        std::string filename;
        std::cout << "Media filename: ";
        std::cin >> filename;
        return {CameraCommandType::remove_media, std::move(filename)};
    }
    case 0:
        return {CameraCommandType::shutdown, {}};
    default:
        throw std::invalid_argument("unknown CLI command");
    }
}

void CliCommandSource::publish_result(const CommandResult& result) {
    if (!result.message.empty()) {
        std::cout << result.message << '\n';
    }
}

}  // namespace camera_manager
