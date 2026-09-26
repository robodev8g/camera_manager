#pragma once

#include "camera_agent/camera_controller.hpp"

#include <csignal>
#include <string>

namespace camera_agent {

class LocalCli final {
public:
    explicit LocalCli(CameraController& controller);

    void run(volatile std::sig_atomic_t& keep_running);

private:
    void execute(const std::string& command,
                 volatile std::sig_atomic_t& keep_running);
    static void print_help();

    CameraController& controller_;
};

}  // namespace camera_agent
