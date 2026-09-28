#include "command_handler.hpp"

#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace camera_manager {

CommandHandler::CommandHandler(ICameraAdapter& camera,
                               std::filesystem::path media_directory)
    : camera_(camera), media_directory_(std::move(media_directory)) {}

std::filesystem::path CommandHandler::make_media_path(
    const char* type,
    const char* extension) const {
    std::filesystem::create_directories(media_directory_);

    const std::time_t now = std::time(nullptr);
    const std::tm local_time = *std::localtime(&now);

    std::ostringstream filename;
    filename << type << '_' << std::put_time(&local_time, "%Y%m%d_%H%M%S")
             << extension;
    return media_directory_ / filename.str();
}

CommandResult CommandHandler::list_media() const {
    std::vector<std::filesystem::path> filenames;
    if (std::filesystem::exists(media_directory_)) {
        for (const auto& entry :
             std::filesystem::directory_iterator(media_directory_)) {
            if (entry.is_regular_file()) {
                filenames.push_back(entry.path().filename());
            }
        }
    }

    std::sort(filenames.begin(), filenames.end());
    if (filenames.empty()) {
        return {"Media directory is empty"};
    }

    std::ostringstream result;
    result << "Media files:";
    for (const auto& filename : filenames) {
        result << "\n- " << filename.string();
    }
    return {result.str()};
}

CommandResult CommandHandler::remove_media(const std::string& name) const {
    const std::filesystem::path filename{name};
    if (filename.empty() || filename == "." || filename == ".." ||
        filename != filename.filename()) {
        throw std::invalid_argument("media name must be a filename");
    }

    if (!std::filesystem::remove(media_directory_ / filename)) {
        throw std::runtime_error("media file not found: " + name);
    }
    return {"Removed " + filename.string()};
}

CommandResult CommandHandler::request_local_preview() {
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (preview_requested_ || preview_active_) {
            return {"Live view is already open"};
        }
        preview_requested_ = true;
    }

    state_changed_.notify_one();
    return {"Live view requested; press q or Esc in its window to close it"};
}

CommandResult CommandHandler::request_shutdown() {
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        shutdown_requested_ = true;
    }
    state_changed_.notify_one();
    return {};
}

CommandResult CommandHandler::handle(const CameraCommand& command) {
    std::lock_guard<std::mutex> command_lock(command_mutex_);

    switch (command.type) {
    case CameraCommandType::take_snapshot: {
        const auto output = make_media_path("photo", ".png");
        camera_.take_snapshot(output);
        return {"Snapshot saved to " + output.string()};
    }
    case CameraCommandType::start_recording: {
        const auto output = make_media_path("video", ".mp4");
        camera_.start_record(output);
        return {"Recording started: " + output.string()};
    }
    case CameraCommandType::stop_recording:
        camera_.stop_record();
        return {"Recording stopped"};
    case CameraCommandType::open_local_preview:
        return request_local_preview();
    case CameraCommandType::list_media:
        return list_media();
    case CameraCommandType::remove_media:
        return remove_media(command.argument);
    case CameraCommandType::shutdown:
        return request_shutdown();
    }

    throw std::invalid_argument("unsupported camera command");
}

void CommandHandler::run(ICommandSource& source) {
    std::thread source_thread([this, &source] {
        try {
            while (true) {
                const CameraCommand command = source.wait_for_command();
                source.publish_result(handle(command));
                if (command.type == CameraCommandType::shutdown) {
                    return;
                }
            }
        } catch (...) {
            {
                std::lock_guard<std::mutex> lock(state_mutex_);
                source_error_ = std::current_exception();
                shutdown_requested_ = true;
            }
            state_changed_.notify_one();
        }
    });

    while (true) {
        std::unique_lock<std::mutex> lock(state_mutex_);
        state_changed_.wait(lock, [this] {
            return preview_requested_ || shutdown_requested_;
        });

        if (shutdown_requested_) {
            break;
        }

        preview_requested_ = false;
        preview_active_ = true;
        lock.unlock();

        camera_.live_view();

        lock.lock();
        preview_active_ = false;
    }

    source_thread.join();
    if (source_error_) {
        std::rethrow_exception(source_error_);
    }
}

}  // namespace camera_manager
