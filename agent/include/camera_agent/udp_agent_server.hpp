#pragma once

#include "camera_agent/agent_config.hpp"
#include "camera_agent/camera_backend.hpp"

#include <atomic>
#include <csignal>
#include <cstdint>
#include <filesystem>
#include <string>

namespace camera_agent {

class UdpAgentServer final {
public:
    UdpAgentServer(AgentConfig config, ICameraBackend& camera);
    ~UdpAgentServer();

    UdpAgentServer(const UdpAgentServer&) = delete;
    UdpAgentServer& operator=(const UdpAgentServer&) = delete;

    void run(const volatile std::sig_atomic_t& keep_running);

private:
    [[nodiscard]] std::string handle_command(const std::string& command);
    [[nodiscard]] std::filesystem::path media_path(const std::string& prefix,
                                                   const std::string& extension)
        const;
    void send_heartbeat(const std::string& state) const;
    void stop_outputs() noexcept;

    AgentConfig config_;
    ICameraBackend& camera_;
    int socket_{-1};
    std::uint32_t client_address_{};
    std::atomic_bool recording_{false};
    std::atomic_bool streaming_{false};
};

}  // namespace camera_agent
