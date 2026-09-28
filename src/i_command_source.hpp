#pragma once

namespace camera_manager {

class CommandQueue;

class ICommandSource {
public:
    virtual ~ICommandSource() = default;

    virtual void run(CommandQueue& command_queue) = 0;
};

}  // namespace camera_manager
