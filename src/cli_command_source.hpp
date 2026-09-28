#pragma once

#include "i_command_source.hpp"

namespace camera_manager {

class CliCommandSource final : public ICommandSource {
public:
    CameraCommand wait_for_command() override;
    void publish_result(const CommandResult& result) override;

private:
    void print_menu() const;
};

}  // namespace camera_manager
