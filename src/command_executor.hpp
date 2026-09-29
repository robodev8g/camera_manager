#pragma once

#include "command_queue.hpp"

#include <atomic>
#include <condition_variable>
#include <mutex>

namespace camera_manager {

class CommandHandler;

class CommandExecutor {
public:
    CommandExecutor(CommandHandler& handler, CommandQueue& command_queue);

    void run();
    void run_main_thread();

private:
    CommandResult execute_safely(const CameraCommand& command) noexcept;
    void request_preview(QueuedCommand command);
    void request_shutdown(const CommandResponder& responder);
    static void respond(const CommandResponder& responder,
                        const CommandResult& result) noexcept;

    CommandHandler& handler_;
    CommandQueue& command_queue_;
    std::mutex state_mutex_;
    std::condition_variable state_changed_;
    bool preview_requested_{false};
    bool preview_active_{false};
    CommandResponder preview_responder_;
    std::atomic_bool shutdown_requested_{false};
};

}  // namespace camera_manager
