#include "command_executor.hpp"

#include "command_handler.hpp"

#include <exception>
#include <utility>

namespace camera_manager {

CommandExecutor::CommandExecutor(CommandHandler& handler,
                                 CommandQueue& command_queue)
    : handler_(handler), command_queue_(command_queue) {}

CommandResult CommandExecutor::execute_safely(
    const CameraCommand& command) noexcept {
    try {
        return handler_.handle(command);
    } catch (const std::exception& error) {
        return {error.what(), false};
    } catch (...) {
        return {"unknown command error", false};
    }
}

void CommandExecutor::respond(const CommandResponder& responder,
                              const CommandResult& result) noexcept {
    if (!responder) {
        return;
    }

    try {
        responder(result);
    } catch (...) {
        // A failed response transport must not stop command execution.
    }
}

void CommandExecutor::request_preview(QueuedCommand command) {
    bool already_open = false;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (preview_requested_ || preview_active_) {
            already_open = true;
        } else {
            preview_requested_ = true;
            preview_responder_ = command.respond;
        }
    }

    if (already_open) {
        respond(command.respond, {"Live view is already open", false});
        return;
    }

    state_changed_.notify_one();
    respond(
        command.respond,
        {"Live view requested; press q or Esc in its window to close it"});
}

void CommandExecutor::request_shutdown(const CommandResponder& responder) {
    shutdown_requested_.store(true);
    state_changed_.notify_one();
    respond(responder, {});
}

void CommandExecutor::run() {
    while (true) {
        QueuedCommand queued_command = command_queue_.wait_and_pop();

        switch (queued_command.command.type) {
        case CameraCommandType::open_local_preview:
            request_preview(std::move(queued_command));
            break;
        case CameraCommandType::shutdown:
            request_shutdown(queued_command.respond);
            return;
        default:
            respond(
                queued_command.respond,
                execute_safely(queued_command.command));
            break;
        }
    }
}

void CommandExecutor::run_main_thread() {
    while (true) {
        CommandResponder preview_responder;
        {
            std::unique_lock<std::mutex> lock(state_mutex_);
            state_changed_.wait(lock, [this] {
                return preview_requested_ || shutdown_requested_.load();
            });

            if (shutdown_requested_.load()) {
                return;
            }

            preview_requested_ = false;
            preview_active_ = true;
            preview_responder = std::move(preview_responder_);
        }

        CommandResult preview_result;
        try {
            handler_.run_local_preview(shutdown_requested_);
        } catch (const std::exception& error) {
            preview_result = {error.what(), false};
        } catch (...) {
            preview_result = {"unknown live-view error", false};
        }

        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            preview_active_ = false;
        }

        if (!preview_result.message.empty()) {
            respond(preview_responder, preview_result);
        }

        if (shutdown_requested_.load()) {
            return;
        }
    }
}

}  // namespace camera_manager
