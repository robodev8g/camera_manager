#pragma once

#include "camera_command.hpp"
#include "i_camera_adapter.hpp"
#include "i_command_source.hpp"

#include <condition_variable>
#include <exception>
#include <filesystem>
#include <mutex>

namespace camera_manager {

class CommandHandler {
public:
    CommandHandler(ICameraAdapter& camera,
                   std::filesystem::path media_directory);

    CommandResult handle(const CameraCommand& command);
    void run(ICommandSource& source);

private:
    std::filesystem::path make_media_path(const char* type,
                                          const char* extension) const;
    CommandResult list_media() const;
    CommandResult remove_media(const std::string& name) const;
    CommandResult request_local_preview();
    CommandResult request_shutdown();

    ICameraAdapter& camera_;
    std::filesystem::path media_directory_;
    std::mutex command_mutex_;
    std::mutex state_mutex_;
    std::condition_variable state_changed_;
    bool preview_requested_{false};
    bool preview_active_{false};
    bool shutdown_requested_{false};
    std::exception_ptr source_error_;
};

}  // namespace camera_manager
