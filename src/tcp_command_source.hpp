#pragma once

#include "i_command_source.hpp"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>

namespace camera_manager {

class TcpCommandSource final : public ICommandSource {
public:
    explicit TcpCommandSource(std::uint16_t port);
    ~TcpCommandSource() override;

    TcpCommandSource(const TcpCommandSource&) = delete;
    TcpCommandSource& operator=(const TcpCommandSource&) = delete;
    TcpCommandSource(TcpCommandSource&&) = delete;
    TcpCommandSource& operator=(TcpCommandSource&&) = delete;

    void run(CommandQueue& command_queue) override;
    void stop() noexcept;

    std::optional<std::string> connected_peer_address() const;

private:
    void serve_client(int client_socket,
                      std::uint64_t connection_id,
                      const std::string& peer_address,
                      CommandQueue& command_queue);
    void send_response(std::uint64_t connection_id,
                       std::string response) noexcept;

    int listen_socket_{-1};
    std::atomic_bool stopping_{false};
    mutable std::mutex socket_mutex_;
    int client_socket_{-1};
    std::uint64_t connection_id_{0};
    std::optional<std::string> peer_address_;
};

}  // namespace camera_manager
