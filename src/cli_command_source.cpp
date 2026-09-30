#include "cli_command_source.hpp"

#include "command_queue.hpp"

#include <iostream>
#include <sstream>
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
    while (true) {
        print_menu();

        std::string input;
        if (!std::getline(std::cin, input)) {
            return {CameraCommandType::shutdown, {}};
        }

        int command = 0;
        std::string extra;
        std::istringstream parser(input);
        if (!(parser >> command) || (parser >> extra)) {
            publish_result(
                {"Invalid command; enter a number from 0 to 6", false});
            continue;
        }

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
            if (!std::getline(std::cin, filename)) {
                return {CameraCommandType::shutdown, {}};
            }
            return {CameraCommandType::remove_media, std::move(filename)};
        }
        case 0:
            return {CameraCommandType::shutdown, {}};
        default:
            publish_result(
                {"Invalid command; enter a number from 0 to 6", false});
            break;
        }
    }
}

void CliCommandSource::publish_result(const CommandResult& result) {
    if (result.message.empty()) {
        return;
    }

    if (!result.success) {
        std::cout << "Error: ";
    }
    std::cout << result.message << '\n';
}

void CliCommandSource::run(CommandQueue& command_queue) {
    while (true) {
        CameraCommand command = wait_for_command();
        const bool shutdown =
            command.type == CameraCommandType::shutdown;

        command_queue.push({
            std::move(command),
            [this](const CommandResult& result) {
                publish_result(result);
            },
        });

        if (shutdown) {
            return;
        }
    }
}

}  // namespace camera_manager
