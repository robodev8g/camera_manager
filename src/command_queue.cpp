#include "command_queue.hpp"

#include <utility>

namespace camera_manager {

void CommandQueue::push(QueuedCommand command) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        commands_.push(std::move(command));
    }
    command_available_.notify_one();
}

QueuedCommand CommandQueue::wait_and_pop() {
    std::unique_lock<std::mutex> lock(mutex_);
    command_available_.wait(lock, [this] { return !commands_.empty(); });

    QueuedCommand command = std::move(commands_.front());
    commands_.pop();
    return command;
}

}  // namespace camera_manager
