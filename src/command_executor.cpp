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

void CommandExecutor::publish_result(const ResultPublisher& publisher,
                                     const CommandResult& result) noexcept {
    if (!publisher) {
        return;
    }

    try {
        publisher(result);
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
            preview_result_publisher_ = command.publish_result;
        }
    }

    if (already_open) {
        publish_result(
            command.publish_result, {"Live view is already open", false});
        return;
    }

    state_changed_.notify_one();
    publish_result(
        command.publish_result,
        {"Live view requested; press q or Esc in its window to close it"});
}

void CommandExecutor::request_shutdown(
    const ResultPublisher& publish_result) {
    shutdown_requested_.store(true);
    state_changed_.notify_one();
    CommandExecutor::publish_result(publish_result, {});
}

void CommandExecutor::run() {
    while (true) {
        QueuedCommand queued_command = command_queue_.wait_and_pop();

        switch (queued_command.command.type) {
        case CameraCommandType::open_local_preview:
            request_preview(std::move(queued_command));
            break;
        case CameraCommandType::shutdown:
            request_shutdown(queued_command.publish_result);
            return;
        default:
            publish_result(
                queued_command.publish_result,
                execute_safely(queued_command.command));
            break;
        }
    }
}

void CommandExecutor::run_main_thread() {
    while (true) {
        ResultPublisher preview_result_publisher;
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
            preview_result_publisher =
                std::move(preview_result_publisher_);
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
            publish_result(preview_result_publisher, preview_result);
        }

        if (shutdown_requested_.load()) {
            return;
        }
    }
}

}  // namespace camera_manager
