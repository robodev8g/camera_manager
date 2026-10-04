#include "zmq_command_source.hpp"

#include "camera_command.hpp"
#include "command_queue.hpp"

#ifdef HAVE_LIBZMQ
#include <zmq.h>
#include <json/json.h>
#include <string>
#include <thread>
#include <vector>

namespace camera_manager {

namespace {

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
                                   const std::string& peer_address,
                                   CameraCommand& command) {
    // Reuse TCP parsing logic where possible; simplified here for brevity.
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
    } else if (name == "start_stream") {
        const Json::Value& arguments = request["arguments"];
        if (!arguments.isObject() || !arguments.isMember("udp_port") ||
            !arguments["udp_port"].isUInt() ||
            arguments["udp_port"].asUInt() == 0 ||
            arguments["udp_port"].asUInt() > 65535) {
            return {"start_stream requires arguments.udp_port from 1 to 65535",
                    false};
        }
        command = {
            CameraCommandType::start_stream,
            peer_address,
            static_cast<std::uint16_t>(arguments["udp_port"].asUInt()),
        };
    } else if (name == "stop_stream") {
        command = {CameraCommandType::stop_stream, {}};
    } else if (name == "list_media") {
        command = {CameraCommandType::list_media, {}};
    } else if (name == "remove_media") {
        const Json::Value& arguments = request["arguments"];
        if (!arguments.isObject() || !arguments.isMember("filename") ||
            !arguments["filename"].isString() ||
            arguments["filename"].asString().empty()) {
            return {"remove_media requires a non-empty arguments.filename",
                    false};
        }
        command = {CameraCommandType::remove_media,
                   arguments["filename"].asString()};
    } else if (name == "shutdown" || name == "open_local_preview") {
        return {"command is not available over ZMQ", false};
    } else {
        return {"unknown command: " + name, false};
    }

    return {};
}

}  // namespace

ZmqCommandSource::ZmqCommandSource(const std::string& endpoint)
    : endpoint_(endpoint) {}

ZmqCommandSource::~ZmqCommandSource() { stop(); }

void ZmqCommandSource::stop() noexcept { stopping_.store(true); }

void ZmqCommandSource::run(CommandQueue& command_queue) {
    void* context = zmq_ctx_new();
    void* socket = zmq_socket(context, ZMQ_REP);
    zmq_bind(socket, endpoint_.c_str());

    std::uint64_t next_request_id = 1;

    while (!stopping_.load()) {
        zmq_msg_t msg;
        zmq_msg_init(&msg);
        const int rc = zmq_msg_recv(&msg, socket, 0);
        if (rc == -1) {
            zmq_msg_close(&msg);
            if (zmq_errno() == ETERM || stopping_.load()) {
                break;
            }
            continue;
        }

        const std::string payload(static_cast<char*>(zmq_msg_data(&msg)),
                                  zmq_msg_size(&msg));
        zmq_msg_close(&msg);

        Json::Value request;
        std::string parse_error;
        if (!parse_json(payload, request, parse_error)) {
            Json::Value resp = make_response(Json::Value::null,
                                             {"invalid JSON: " + parse_error, false});
            const std::string text = serialize_json(resp);
            zmq_send(socket, text.data(), text.size(), 0);
            continue;
        }

        Json::Value id = Json::Value::null;
        if (request.isObject() && request.isMember("id")) {
            id = request["id"];
        }
        if (!id.isUInt64()) {
            Json::Value resp = make_response(Json::Value::null,
                                             {"id must be an unsigned integer", false});
            const std::string text = serialize_json(resp);
            zmq_send(socket, text.data(), text.size(), 0);
            continue;
        }

            CameraCommand command;
            // prefer explicit client_ip in arguments when present
            std::string client_address = "zmq_peer";
            if (request.isObject() && request.isMember("arguments") && request["arguments"].isObject() && request["arguments"].isMember("client_ip") && request["arguments"]["client_ip"].isString()) {
                client_address = request["arguments"]["client_ip"].asString();
            }
            const CommandResult parsed = parse_remote_command(request, client_address, command);
        if (!parsed.success) {
            Json::Value resp = make_response(id, parsed);
            const std::string text = serialize_json(resp);
            zmq_send(socket, text.data(), text.size(), 0);
            continue;
        }

        const std::uint64_t request_id = id.asUInt64();

        command_queue.push({
            std::move(command),
            [socket, request_id](const CommandResult& result) {
                Json::Value response = make_response(Json::Value(request_id), result);
                std::string text = serialize_json(response);
                zmq_send(socket, text.data(), text.size(), 0);
            },
        });

    }

    zmq_close(socket);
    zmq_ctx_destroy(context);
}

}  // namespace camera_manager

#else

// Fallback stub if ZMQ unavailable at build time.
ZmqCommandSource::ZmqCommandSource(const std::string&) { }
ZmqCommandSource::~ZmqCommandSource() = default;
void ZmqCommandSource::run(CommandQueue&) { throw std::runtime_error("ZMQ not available"); }
void ZmqCommandSource::stop() noexcept {}

#endif
