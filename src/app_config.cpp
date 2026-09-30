#include "app_config.hpp"

#include <json/json.h>

#include <fstream>
#include <stdexcept>
#include <string>

namespace camera_manager {

AppConfig load_config(const std::filesystem::path& config_path) {
    std::ifstream input(config_path);
    if (!input) {
        throw std::runtime_error("could not open config file: " +
                                 config_path.string());
    }

    Json::CharReaderBuilder reader;
    Json::Value root;
    std::string errors;
    if (!Json::parseFromStream(reader, input, &root, &errors)) {
        throw std::runtime_error("invalid JSON config: " + errors);
    }

    if (!root.isObject()) {
        throw std::runtime_error("config root must be a JSON object");
    }
    if (!root.isMember("camera_device_index") ||
        !root["camera_device_index"].isInt()) {
        throw std::runtime_error("camera_device_index must be an integer");
    }
    if (!root.isMember("media_directory") ||
        !root["media_directory"].isString() ||
        root["media_directory"].asString().empty()) {
        throw std::runtime_error("media_directory must be a non-empty string");
    }
    if (!root.isMember("control_port") ||
        !root["control_port"].isInt() ||
        root["control_port"].asInt() < 1 ||
        root["control_port"].asInt() > 65535) {
        throw std::runtime_error(
            "control_port must be an integer from 1 to 65535");
    }

    for (const auto& key : root.getMemberNames()) {
        if (key != "camera_device_index" && key != "media_directory" &&
            key != "control_port") {
            throw std::runtime_error("unknown config key: " + key);
        }
    }

    return {
        root["camera_device_index"].asInt(),
        root["media_directory"].asString(),
        static_cast<std::uint16_t>(root["control_port"].asInt()),
    };
}

}  // namespace camera_manager
