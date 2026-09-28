#pragma once

#include "i_camera_adapter.hpp"

#include <condition_variable>
#include <exception>
#include <filesystem>
#include <mutex>

namespace camera_manager {

class CameraCli {
public:
    CameraCli(ICameraAdapter& camera, std::filesystem::path media_dir_path);

    void run();

private:
    void print_menu() const;
    void list_media() const;
    void remove_media() const;
    void read_commands();
    void request_live_view();

    ICameraAdapter& camera_;
    std::filesystem::path media_dir_path_;
    std::mutex state_mutex_;
    std::condition_variable state_changed_;
    bool live_view_requested_{false};
    bool live_view_active_{false};
    bool exit_requested_{false};
    std::exception_ptr command_error_;
};

}  // namespace camera_manager
