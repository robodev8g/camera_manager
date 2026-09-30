#include "tcp_command_source.hpp"

#include "camera_command.hpp"
#include "command_queue.hpp"

#include <arpa/inet.h>
#include <json/json.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <limits>
#include <memory>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace camera_manager {
namespace {

constexpr std::uint32_t maximum_message_size = 64U * 1024U;
constexpr int accept_poll_timeout_ms = 250;

std::system_error socket_error(const char* operation) {
    return {errno, std::generic_category(), operation};
}

bool receive_exact(int socket, void* destination, std::size_t size) {
    auto* output = static_cast<unsigned char*>(destination);
    std::size_t received = 0;

    while (received < size) {
        const ssize_t count =
            ::recv(socket, output + received, size - received, 0);
        if (count == 0) {
            return false;
        }
        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw socket_error("could not receive TCP message");
        }
        received += static_cast<std::size_t>(count);
    }
    return true;
}

bool send_exact(int socket, const void* source, std::size_t size) {
    const auto* input = static_cast<const unsigned char*>(source);
    std::size_t sent = 0;

    while (sent < size) {
        const ssize_t count = ::send(
            socket, input + sent, size - sent, MSG_NOSIGNAL);
        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        if (count == 0) {
            return false;
        }
        sent += static_cast<std::size_t>(count);
    }
    return true;
}

std::optional<std::string> receive_frame(int socket) {
    std::uint32_t network_size = 0;
    if (!receive_exact(socket, &network_size, sizeof(network_size))) {
        return std::nullopt;
    }

    const std::uint32_t message_size = ntohl(network_size);
    if (message_size == 0 || message_size > maximum_message_size) {
        throw std::runtime_error("invalid TCP message length");
    }

    std::string payload(message_size, '\0');
    if (!receive_exact(socket, payload.data(), payload.size())) {
        throw std::runtime_error("connection closed during TCP message");
    }
    return payload;
}

bool send_frame(int socket, const std::string& payload) {
    if (payload.size() > std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }

    const auto payload_size = static_cast<std::uint32_t>(payload.size());
    const std::uint32_t network_size = htonl(payload_size);
    return send_exact(socket, &network_size, sizeof(network_size)) &&
           send_exact(socket, payload.data(), payload.size());
}

std::string serialize_json(const Json::Value& value) {
    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";
    return Json::writeString(writer, value);
}

bool parse_json(const std::string& payload,
                Json::Value& value,
                std::string& error) {
    Json::CharReaderBuilder reader;
    const std::unique_ptr<Json::CharReader> parser(reader.newCharReader());
    return parser->parse(
        payload.data(), payload.data() + payload.size(), &value, &error);
}

Json::Value make_response(const Json::Value& id,
                          const CommandResult& result) {
    Json::Value response(Json::objectValue);
    response["id"] = id;
    response["success"] = result.success;
    response["message"] = result.message;
    return response;
}

CommandResult parse_remote_command(const Json::Value& request,
                                   CameraCommand& command) {
    if (!request.isObject()) {
        return {"request must be a JSON object", false};
    }
    if (!request.isMember("command") || !request["command"].isString()) {
        return {"command must be a string", false};
    }

    const std::string name = request["command"].asString();
    if (name == "take_snapshot") {
        command = {CameraCommandType::take_snapshot, {}};
    } else if (name == "start_recording") {
        command = {CameraCommandType::start_recording, {}};
    } else if (name == "stop_recording") {
        command = {CameraCommandType::stop_recording, {}};
    } else if (name == "list_media") {
        command = {CameraCommandType::list_media, {}};
    } else if (name == "remove_media") {
        const Json::Value& arguments = request["arguments"];
        if (!arguments.isObject() ||
            !arguments.isMember("filename") ||
            !arguments["filename"].isString() ||
            arguments["filename"].asString().empty()) {
            return {
                "remove_media requires a non-empty arguments.filename",
                false,
            };
        }
        command = {
            CameraCommandType::remove_media,
            arguments["filename"].asString(),
        };
    } else if (name == "shutdown" || name == "open_local_preview") {
        return {"command is not available over TCP", false};
    } else {
        return {"unknown command: " + name, false};
    }

    return {};
}

}  // namespace

TcpCommandSource::TcpCommandSource(std::uint16_t port) {
    listen_socket_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_socket_ < 0) {
        throw socket_error("could not create TCP control socket");
    }

    try {
        int reuse_address = 1;
        if (::setsockopt(
                listen_socket_, SOL_SOCKET, SO_REUSEADDR,
                &reuse_address, sizeof(reuse_address)) < 0) {
            throw socket_error("could not configure TCP control socket");
        }

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_ANY);
        address.sin_port = htons(port);
        if (::bind(
                listen_socket_, reinterpret_cast<sockaddr*>(&address),
                sizeof(address)) < 0) {
            throw socket_error("could not bind TCP control socket");
        }
        if (::listen(listen_socket_, 1) < 0) {
            throw socket_error("could not listen on TCP control socket");
        }
    } catch (...) {
        ::close(listen_socket_);
        listen_socket_ = -1;
        throw;
    }
}

TcpCommandSource::~TcpCommandSource() {
    stop();
    if (listen_socket_ >= 0) {
        ::close(listen_socket_);
    }
}

void TcpCommandSource::stop() noexcept {
    stopping_.store(true);

    std::lock_guard<std::mutex> lock(socket_mutex_);
    if (client_socket_ >= 0) {
        ::shutdown(client_socket_, SHUT_RDWR);
    }
}

std::optional<std::string> TcpCommandSource::connected_peer_address() const {
    std::lock_guard<std::mutex> lock(socket_mutex_);
    return peer_address_;
}

void TcpCommandSource::send_response(
    std::uint64_t connection_id,
    std::string response) noexcept {
    std::lock_guard<std::mutex> lock(socket_mutex_);
    if (client_socket_ < 0 || connection_id != connection_id_) {
        return;
    }
    if (!send_frame(client_socket_, response)) {
        ::shutdown(client_socket_, SHUT_RDWR);
    }
}

void TcpCommandSource::serve_client(
    int client_socket,
    std::uint64_t connection_id,
    CommandQueue& command_queue) {
    try {
        while (!stopping_.load()) {
            const std::optional<std::string> payload =
                receive_frame(client_socket);
            if (!payload) {
                break;
            }

            Json::Value request;
            std::string parse_error;
            if (!parse_json(*payload, request, parse_error)) {
                send_response(
                    connection_id,
                    serialize_json(make_response(
                        Json::Value::null,
                        {"invalid JSON: " + parse_error, false})));
                continue;
            }

            Json::Value id = Json::Value::null;
            if (request.isObject() && request.isMember("id")) {
                id = request["id"];
            }
            if (!id.isUInt64()) {
                send_response(
                    connection_id,
                    serialize_json(make_response(
                        Json::Value::null,
                        {"id must be an unsigned integer", false})));
                continue;
            }

            CameraCommand command;
            const CommandResult parsed = parse_remote_command(request, command);
            if (!parsed.success) {
                send_response(
                    connection_id,
                    serialize_json(make_response(id, parsed)));
                continue;
            }

            const std::uint64_t request_id = id.asUInt64();
            command_queue.push({
                std::move(command),
                [this, connection_id, request_id](
                    const CommandResult& result) {
                    send_response(
                        connection_id,
                        serialize_json(make_response(request_id, result)));
                },
            });
        }
    } catch (const std::exception&) {
        // A malformed frame or socket failure closes only this connection.
    }
}

void TcpCommandSource::run(CommandQueue& command_queue) {
    while (!stopping_.load()) {
        pollfd descriptor{listen_socket_, POLLIN, 0};
        const int ready = ::poll(&descriptor, 1, accept_poll_timeout_ms);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw socket_error("could not wait for TCP controller");
        }
        if (ready == 0) {
            continue;
        }

        sockaddr_in peer{};
        socklen_t peer_size = sizeof(peer);
        const int client = ::accept(
            listen_socket_, reinterpret_cast<sockaddr*>(&peer), &peer_size);
        if (client < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw socket_error("could not accept TCP controller");
        }

        char peer_text[INET_ADDRSTRLEN]{};
        const char* converted = ::inet_ntop(
            AF_INET, &peer.sin_addr, peer_text, sizeof(peer_text));
        std::string peer_address = converted ? converted : "unknown";
        std::uint64_t connection_id = 0;

        {
            std::lock_guard<std::mutex> lock(socket_mutex_);
            client_socket_ = client;
            connection_id = ++connection_id_;
            peer_address_ = std::move(peer_address);
        }

        serve_client(client, connection_id, command_queue);

        {
            std::lock_guard<std::mutex> lock(socket_mutex_);
            if (connection_id_ == connection_id) {
                ::close(client_socket_);
                client_socket_ = -1;
                peer_address_.reset();
            }
        }
    }
}

}  // namespace camera_manager
