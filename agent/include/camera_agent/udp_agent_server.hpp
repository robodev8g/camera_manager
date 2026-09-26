#pragma once

#include "camera_agent/agent_config.hpp"
#include "camera_agent/camera_controller.hpp"

#include <csignal>
#include <cstdint>
#include <string>

namespace camera_agent {

class UdpAgentServer final {
public:
    UdpAgentServer(AgentConfig config, CameraController& controller);
    ~UdpAgentServer();

    UdpAgentServer(const UdpAgentServer&) = delete;
    UdpAgentServer& operator=(const UdpAgentServer&) = delete;

    void run(const volatile std::sig_atomic_t& keep_running);

private:
    [[nodiscard]] std::string handle_command(const std::string& command);
    void send_heartbeat(const std::string& state) const;

    AgentConfig config_;
    CameraController& controller_;
    int socket_{-1};
    std::uint32_t client_address_{};
};

}  // namespace camera_agent
