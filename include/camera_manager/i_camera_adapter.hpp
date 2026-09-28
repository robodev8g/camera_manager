#pragma once

#include <filesystem>

namespace camera_manager {

class ICameraAdapter {
public:
    virtual ~ICameraAdapter();

    virtual void take_snapshot(const std::filesystem::path& output) = 0;
    virtual void start_record(const std::filesystem::path& output) = 0;
    virtual void stop_record() = 0;
    virtual void live_view() = 0;
};

}  // namespace camera_manager
