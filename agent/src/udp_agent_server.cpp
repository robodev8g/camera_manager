#include "camera_agent/udp_agent_server.hpp"

#include "camera_agent/camera_types.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <chrono>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <sstream>
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

UdpAgentServer::UdpAgentServer(AgentConfig config, ICameraBackend& camera)
    : config_(std::move(config)), camera_(camera) {
    client_address_ =
        endpoint(config_.client_host, config_.heartbeat_port).sin_addr.s_addr;
    std::filesystem::create_directories(config_.media_directory);

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
    stop_outputs();
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
    stop_outputs();
    send_heartbeat("OFFLINE");
}

std::string UdpAgentServer::handle_command(const std::string& command) {
    if (command == "PHOTO") {
        const auto path = media_path("photo", ".jpg");
        camera_.take_photo(path);
        return "OK PHOTO " + path.string();
    }
    if (command == "RECORD_START") {
        if (recording_) {
            throw CameraError(CameraErrorCode::conflict,
                              "Recording is already active");
        }
        const auto path = media_path("video", ".mp4");
        camera_.start_recording(path);
        recording_ = true;
        return "OK RECORDING " + path.string();
    }
    if (command == "RECORD_STOP") {
        camera_.stop_recording();
        recording_ = false;
        return "OK RECORDING_STOPPED";
    }
    if (command == "STREAM_START") {
        if (streaming_) {
            throw CameraError(CameraErrorCode::conflict,
                              "Stream is already active");
        }
        camera_.start_live_stream({
            .host = config_.client_host,
            .port = config_.stream_port,
        });
        streaming_ = true;
        return "OK STREAMING";
    }
    if (command == "STREAM_STOP") {
        camera_.stop_live_stream();
        streaming_ = false;
        return "OK STREAM_STOPPED";
    }
    if (command == "STATUS") {
        return "OK STATUS recording=" + std::to_string(recording_.load()) +
               " streaming=" + std::to_string(streaming_.load());
    }
    throw CameraError(CameraErrorCode::invalid_argument,
                      "Unknown command: " + command);
}

std::filesystem::path UdpAgentServer::media_path(
    const std::string& prefix, const std::string& extension) const {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  now.time_since_epoch()) %
                              1000;
    std::tm local{};
    localtime_r(&time, &local);

    std::ostringstream name;
    name << prefix << '_' << std::put_time(&local, "%Y%m%d_%H%M%S") << '_'
         << std::setfill('0') << std::setw(3) << milliseconds.count()
         << extension;
    return config_.media_directory / name.str();
}

void UdpAgentServer::send_heartbeat(const std::string& state) const {
    const auto destination =
        endpoint(config_.client_host, config_.heartbeat_port);
    const std::string message =
        "CAMERA_MANAGER|" + state +
        "|recording=" + std::to_string(recording_.load()) +
        "|streaming=" + std::to_string(streaming_.load()) +
        "|command_port=" + std::to_string(config_.command_port) +
        "|stream_port=" + std::to_string(config_.stream_port);
    sendto(socket_, message.data(), message.size(), 0,
           reinterpret_cast<const sockaddr*>(&destination),
           sizeof(destination));
}

void UdpAgentServer::stop_outputs() noexcept {
    try {
        if (recording_.exchange(false)) {
            camera_.stop_recording();
        }
    } catch (...) {
    }
    try {
        if (streaming_.exchange(false)) {
            camera_.stop_live_stream();
        }
    } catch (...) {
    }
}

}  // namespace camera_agent
