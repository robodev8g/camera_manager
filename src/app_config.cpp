#include "app_config.hpp"

#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>

namespace camera_manager {
namespace {

std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }

    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

}  // namespace

AppConfig load_config(const std::filesystem::path& config_path) {
    std::ifstream input(config_path);
    if (!input) {
        throw std::runtime_error("could not open config file: " +
                                 config_path.string());
    }

    std::optional<int> camera_device_index;
    std::optional<std::filesystem::path> media_directory;
    std::string line;
    std::size_t line_number = 0;

    while (std::getline(input, line)) {
        ++line_number;
        line = trim(line);
        if (line.empty() || line.front() == '#') {
            continue;
        }

        const auto separator = line.find('=');
        if (separator == std::string::npos) {
            throw std::runtime_error("invalid config line " +
                                     std::to_string(line_number));
        }

        const std::string key = trim(line.substr(0, separator));
        const std::string value = trim(line.substr(separator + 1));

        if (key == "camera_device_index") {
            std::size_t parsed_characters = 0;
            camera_device_index = std::stoi(value, &parsed_characters);
            if (parsed_characters != value.size()) {
                throw std::runtime_error("invalid camera_device_index");
            }
        } else if (key == "media_directory") {
            if (value.empty()) {
                throw std::runtime_error("media_directory must not be empty");
            }
            media_directory = value;
        } else {
            throw std::runtime_error("unknown config key: " + key);
        }
    }

    if (!camera_device_index || !media_directory) {
        throw std::runtime_error("config file is missing required values");
    }

    return {*camera_device_index, *media_directory};
}

}  // namespace camera_manager
