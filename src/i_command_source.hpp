#pragma once

#include "camera_command.hpp"

namespace camera_manager {

class ICommandSource {
public:
    virtual ~ICommandSource() = default;

    virtual CameraCommand wait_for_command() = 0;
    virtual void publish_result(const CommandResult& result) = 0;
};

}  // namespace camera_manager
