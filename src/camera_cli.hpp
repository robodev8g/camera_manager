#pragma once

#include "i_camera_adapter.hpp"

namespace camera_manager {

class CameraCli {
public:
    explicit CameraCli(ICameraAdapter& camera);

    void run();

private:
    void print_menu() const;

    ICameraAdapter& camera_;
};

}  // namespace camera_manager
