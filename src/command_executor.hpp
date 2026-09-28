#pragma once

#include "command_queue.hpp"

#include <atomic>

namespace camera_manager {

class CommandHandler;
class MainThreadTaskQueue;

class CommandExecutor {
public:
    CommandExecutor(CommandHandler& handler,
                    CommandQueue& command_queue,
                    MainThreadTaskQueue& main_thread_tasks);

    void run();

private:
    CommandResult execute_safely(const CameraCommand& command) noexcept;
    void schedule_preview(QueuedCommand command);
    static void respond(const CommandResponder& responder,
                        const CommandResult& result) noexcept;

    CommandHandler& handler_;
    CommandQueue& command_queue_;
    MainThreadTaskQueue& main_thread_tasks_;
    std::atomic_bool preview_open_{false};
};

}  // namespace camera_manager
