#include "camera_cli.hpp"

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace camera_manager {
namespace {

std::filesystem::path make_media_path(const std::filesystem::path& directory,
                                      const char* type,
                                      const char* extension) {
    std::filesystem::create_directories(directory);

    const std::time_t now = std::time(nullptr);
    const std::tm local_time = *std::localtime(&now);

    std::ostringstream filename;
    filename << type << '_' << std::put_time(&local_time, "%Y%m%d_%H%M%S")
             << extension;
    return directory / filename.str();
}

}  // namespace

CameraCli::CameraCli(ICameraAdapter& camera,
                     std::filesystem::path media_dir_path)
    : camera_(camera), media_dir_path_(std::move(media_dir_path)) {}

void CameraCli::print_menu() const {
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

void CameraCli::list_media() const {
    std::vector<std::filesystem::path> filenames;
    if (std::filesystem::exists(media_dir_path_)) {
        for (const auto& entry :
             std::filesystem::directory_iterator(media_dir_path_)) {
            if (entry.is_regular_file()) {
                filenames.push_back(entry.path().filename());
            }
        }
    }

    std::sort(filenames.begin(), filenames.end());
    if (filenames.empty()) {
        std::cout << "Media directory is empty\n";
        return;
    }

    std::cout << "Media files:\n";
    for (const auto& filename : filenames) {
        std::cout << "- " << filename.string() << '\n';
    }
}

void CameraCli::remove_media() const {
    std::string name;
    std::cout << "Media filename: ";
    std::cin >> name;

    const std::filesystem::path filename{name};
    if (filename.empty() || filename == "." || filename == ".." ||
        filename != filename.filename()) {
        throw std::invalid_argument("media name must be a filename");
    }

    const auto media_path = media_dir_path_ / filename;
    if (!std::filesystem::remove(media_path)) {
        throw std::runtime_error("media file not found: " + name);
    }
    std::cout << "Removed " << filename.string() << '\n';
}

void CameraCli::request_live_view() {
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (live_view_requested_ || live_view_active_) {
            std::cout << "Live view is already open\n";
            return;
        }
        live_view_requested_ = true;
    }

    std::cout << "Live view requested; press q or Esc in its window to close it\n";
    state_changed_.notify_one();
}

void CameraCli::read_commands() {
    while (true) {
        print_menu();

        int command = 0;
        std::cin >> command;

        switch (command) {
        case 1: {
            const auto output =
                make_media_path(media_dir_path_, "photo", ".png");
            camera_.take_snapshot(output);
            std::cout << "Snapshot saved to " << output << '\n';
            break;
        }
        case 2: {
            const auto output =
                make_media_path(media_dir_path_, "video", ".mp4");
            camera_.start_record(output);
            std::cout << "Recording started: " << output << '\n';
            break;
        }
        case 3:
            camera_.stop_record();
            std::cout << "Recording stopped\n";
            break;
        case 4:
            request_live_view();
            break;
        case 5:
            list_media();
            break;
        case 6:
            remove_media();
            break;
        case 0: {
            std::lock_guard<std::mutex> lock(state_mutex_);
            exit_requested_ = true;
            state_changed_.notify_one();
            return;
        }
        }
    }
}

void CameraCli::run() {
    std::thread command_thread([this] {
        try {
            read_commands();
        } catch (...) {
            {
                std::lock_guard<std::mutex> lock(state_mutex_);
                command_error_ = std::current_exception();
                exit_requested_ = true;
            }
            state_changed_.notify_one();
        }
    });

    while (true) {
        std::unique_lock<std::mutex> lock(state_mutex_);
        state_changed_.wait(lock, [this] {
            return live_view_requested_ || exit_requested_;
        });

        if (exit_requested_) {
            break;
        }

        live_view_requested_ = false;
        live_view_active_ = true;
        lock.unlock();

        camera_.live_view();

        lock.lock();
        live_view_active_ = false;
    }

    command_thread.join();
    if (command_error_) {
        std::rethrow_exception(command_error_);
    }
}

}  // namespace camera_manager
