#pragma once

#include "camera_command.hpp"
#include "i_command_source.hpp"

namespace camera_manager {

class CliCommandSource final : public ICommandSource {
public:
    void run(CommandQueue& command_queue) override;

private:
    CameraCommand wait_for_command();
    void publish_result(const CommandResult& result);
    void print_menu() const;
};

}  // namespace camera_manager
