#pragma once

namespace camera_manager {

class CommandQueue;

class ICommandSource {
public:
    virtual ~ICommandSource() = default;

    virtual void run(CommandQueue& command_queue) = 0;

    // Optional stop hook for sources that need orderly shutdown.
    virtual void stop() noexcept {}
};

}  // namespace camera_manager
