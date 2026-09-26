#include "camera_agent/local_cli.hpp"

#include <poll.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <iostream>
#include <string>

namespace camera_agent {
namespace {

[[nodiscard]] std::string normalize(std::string command) {
    const auto first = command.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = command.find_last_not_of(" \t\r\n");
    command = command.substr(first, last - first + 1);
    std::transform(command.begin(), command.end(), command.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return command;
}

}  // namespace

LocalCli::LocalCli(CameraController& controller) : controller_(controller) {}

void LocalCli::run(volatile std::sig_atomic_t& keep_running) {
    std::cout << "Local camera CLI enabled. Type 'help' for commands.\n";
    std::cout << "camera> " << std::flush;

    while (keep_running != 0) {
        pollfd input{
            .fd = STDIN_FILENO,
            .events = POLLIN,
            .revents = 0,
        };
        const int result = ::poll(&input, 1, 250);
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw CameraError(CameraErrorCode::io_error,
                              "Cannot read local CLI input");
        }
        if (result == 0) {
            continue;
        }
        if ((input.revents & (POLLERR | POLLNVAL)) != 0) {
            throw CameraError(CameraErrorCode::io_error,
                              "Local CLI input is unavailable");
        }
        if ((input.revents & (POLLIN | POLLHUP)) == 0) {
            continue;
        }

        std::string command;
        if (!std::getline(std::cin, command)) {
            keep_running = 0;
            break;
        }
        try {
            execute(normalize(std::move(command)), keep_running);
        } catch (const std::exception& error) {
            std::cout << "ERROR " << error.what() << '\n';
        }
        if (keep_running != 0) {
            std::cout << "camera> " << std::flush;
        }
    }
}

void LocalCli::execute(const std::string& command,
                       volatile std::sig_atomic_t& keep_running) {
    if (command.empty()) {
        return;
    }
    if (command == "photo") {
        const auto path = controller_.take_photo();
        std::cout << "OK photo saved to " << path << '\n';
        return;
    }
    if (command == "record start") {
        const auto path = controller_.start_recording();
        std::cout << "OK recording to " << path << '\n';
        return;
    }
    if (command == "record stop") {
        controller_.stop_recording();
        std::cout << "OK recording stopped\n";
        return;
    }
    if (command == "preview start" || command == "stream start") {
        controller_.start_local_preview();
        std::cout << "OK local preview started\n";
        return;
    }
    if (command == "preview stop" || command == "stream stop") {
        controller_.stop_local_preview();
        std::cout << "OK local preview stopped\n";
        return;
    }
    if (command == "status") {
        const auto status = controller_.status();
        std::cout << "OK recording=" << status.recording
                  << " local_preview=" << status.local_preview
                  << " network_streaming=" << status.network_streaming << '\n';
        return;
    }
    if (command == "help") {
        print_help();
        return;
    }
    if (command == "quit" || command == "exit") {
        keep_running = 0;
        return;
    }
    throw CameraError(CameraErrorCode::invalid_argument,
                      "Unknown command: " + command);
}

void LocalCli::print_help() {
    std::cout << "Commands:\n"
              << "  photo          Save the latest frame as JPEG\n"
              << "  record start   Start local MP4 recording\n"
              << "  record stop    Stop local MP4 recording\n"
              << "  preview start  Open the local camera window\n"
              << "  preview stop   Close the local camera window\n"
              << "  stream start   Alias for preview start\n"
              << "  stream stop    Alias for preview stop\n"
              << "  status         Show active outputs\n"
              << "  quit           Stop the agent\n";
}

}  // namespace camera_agent
