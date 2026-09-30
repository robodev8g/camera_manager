#pragma once

#include "camera_command.hpp"

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>

namespace camera_manager {

using ResultPublisher = std::function<void(const CommandResult&)>;

struct QueuedCommand {
    CameraCommand command;
    ResultPublisher publish_result;
};

class CommandQueue {
public:
    void push(QueuedCommand command);
    QueuedCommand wait_and_pop();

private:
    std::mutex mutex_;
    std::condition_variable command_available_;
    std::queue<QueuedCommand> commands_;
};

}  // namespace camera_manager
