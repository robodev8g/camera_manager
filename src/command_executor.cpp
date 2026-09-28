#include "command_executor.hpp"

#include "command_handler.hpp"
#include "main_thread_task_queue.hpp"

#include <exception>
#include <utility>

namespace camera_manager {

CommandExecutor::CommandExecutor(CommandHandler& handler,
                                 CommandQueue& command_queue,
                                 MainThreadTaskQueue& main_thread_tasks)
    : handler_(handler),
      command_queue_(command_queue),
      main_thread_tasks_(main_thread_tasks) {}

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

void CommandExecutor::schedule_preview(QueuedCommand command) {
    bool expected = false;
    if (!preview_open_.compare_exchange_strong(expected, true)) {
        respond(command.respond, {"Live view is already open", false});
        return;
    }

    const CommandResponder responder = std::move(command.respond);
    respond(
        responder,
        {"Live view requested; press q or Esc in its window to close it"});

    try {
        main_thread_tasks_.post([this, responder] {
            const CommandResult result = execute_safely(
                {CameraCommandType::open_local_preview, {}});
            preview_open_ = false;

            if (!result.success) {
                respond(responder, result);
            }
        });
    } catch (const std::exception& error) {
        preview_open_ = false;
        respond(responder, {error.what(), false});
    } catch (...) {
        preview_open_ = false;
        respond(responder, {"could not schedule live view", false});
    }
}

void CommandExecutor::run() {
    while (true) {
        QueuedCommand queued_command = command_queue_.wait_and_pop();

        if (queued_command.command.type == CameraCommandType::shutdown) {
            respond(queued_command.respond, {});
            main_thread_tasks_.request_stop();
            return;
        }

        if (queued_command.command.type ==
            CameraCommandType::open_local_preview) {
            schedule_preview(std::move(queued_command));
            continue;
        }

        respond(
            queued_command.respond,
            execute_safely(queued_command.command));
    }
}

}  // namespace camera_manager
