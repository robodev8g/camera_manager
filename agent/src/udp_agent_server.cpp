#include "camera_agent/udp_agent_server.hpp"

#include "camera_agent/camera_types.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cstring>
#include <string>
#include <thread>
#include <utility>

namespace camera_agent {
namespace {

[[nodiscard]] sockaddr_in endpoint(const std::string& host,
                                   std::uint16_t port) {
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &address.sin_addr) != 1) {
        throw CameraError(CameraErrorCode::invalid_argument,
                          "client_host must be an IPv4 address");
    }
    return address;
}

}  // namespace

UdpAgentServer::UdpAgentServer(AgentConfig config,
                               CameraController& controller)
    : config_(std::move(config)), controller_(controller) {
    client_address_ =
        endpoint(config_.client_host, config_.heartbeat_port).sin_addr.s_addr;

    socket_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_ < 0) {
        throw CameraError(CameraErrorCode::io_error,
                          std::string("Cannot create UDP socket: ") +
                              std::strerror(errno));
    }

    const int enabled = 1;
    setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled));
    const timeval timeout{.tv_sec = 0, .tv_usec = 250'000};
    setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_port = htons(config_.command_port);
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(socket_, reinterpret_cast<sockaddr*>(&local), sizeof(local)) < 0) {
        const std::string message = std::strerror(errno);
        ::close(socket_);
        socket_ = -1;
        throw CameraError(CameraErrorCode::io_error,
                          "Cannot bind command port: " + message);
    }
}

UdpAgentServer::~UdpAgentServer() {
    if (socket_ >= 0) {
        ::close(socket_);
    }
}

void UdpAgentServer::run(const volatile std::sig_atomic_t& keep_running) {
    std::jthread heartbeat([this, &keep_running](std::stop_token stop_token) {
        while (!stop_token.stop_requested() && keep_running != 0) {
            send_heartbeat("ONLINE");
            std::this_thread::sleep_for(config_.heartbeat_interval);
        }
    });

    std::array<char, 1024> buffer{};
    while (keep_running != 0) {
        sockaddr_in peer{};
        socklen_t peer_size = sizeof(peer);
        const auto received = recvfrom(
            socket_, buffer.data(), buffer.size() - 1, 0,
            reinterpret_cast<sockaddr*>(&peer), &peer_size);
        if (received < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                continue;
            }
            throw CameraError(CameraErrorCode::io_error,
                              std::string("Cannot receive command: ") +
                                  std::strerror(errno));
        }

        buffer[static_cast<std::size_t>(received)] = '\0';
        if (peer.sin_addr.s_addr != client_address_) {
            continue;
        }
        std::string response;
        try {
            response = handle_command(buffer.data());
        } catch (const std::exception& error) {
            response = "ERROR " + std::string(error.what());
        }
        sendto(socket_, response.data(), response.size(), 0,
               reinterpret_cast<const sockaddr*>(&peer), peer_size);
    }

    heartbeat.request_stop();
    controller_.stop_outputs();
    send_heartbeat("OFFLINE");
}

std::string UdpAgentServer::handle_command(const std::string& command) {
    if (command == "PHOTO") {
        const auto path = controller_.take_photo();
        return "OK PHOTO " + path.string();
    }
    if (command == "RECORD_START") {
        const auto path = controller_.start_recording();
        return "OK RECORDING " + path.string();
    }
    if (command == "RECORD_STOP") {
        controller_.stop_recording();
        return "OK RECORDING_STOPPED";
    }
    if (command == "STREAM_START") {
        controller_.start_network_stream();
        return "OK STREAMING";
    }
    if (command == "STREAM_STOP") {
        controller_.stop_network_stream();
        return "OK STREAM_STOPPED";
    }
    if (command == "STATUS") {
        const auto status = controller_.status();
        return "OK STATUS recording=" + std::to_string(status.recording) +
               " streaming=" +
               std::to_string(status.network_streaming);
    }
    throw CameraError(CameraErrorCode::invalid_argument,
                      "Unknown command: " + command);
}

void UdpAgentServer::send_heartbeat(const std::string& state) const {
    const auto destination =
        endpoint(config_.client_host, config_.heartbeat_port);
    const auto status = controller_.status();
    const std::string message =
        "CAMERA_MANAGER|" + state +
        "|recording=" + std::to_string(status.recording) +
        "|streaming=" + std::to_string(status.network_streaming) +
        "|command_port=" + std::to_string(config_.command_port) +
        "|stream_port=" + std::to_string(config_.stream_port);
    sendto(socket_, message.data(), message.size(), 0,
           reinterpret_cast<const sockaddr*>(&destination),
           sizeof(destination));
}

}  // namespace camera_agent
