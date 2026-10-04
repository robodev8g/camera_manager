#pragma once

#include "i_command_source.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace camera_manager {

class ZmqCommandSource final : public ICommandSource {
public:
    explicit ZmqCommandSource(const std::string& endpoint);
    ~ZmqCommandSource() override;

    ZmqCommandSource(const ZmqCommandSource&) = delete;
    ZmqCommandSource& operator=(const ZmqCommandSource&) = delete;
    ZmqCommandSource(ZmqCommandSource&&) = delete;
    ZmqCommandSource& operator=(ZmqCommandSource&&) = delete;

    void run(CommandQueue& command_queue) override;
    void stop() noexcept override;

private:
    std::string endpoint_;
    std::atomic_bool stopping_{false};
};

}  // namespace camera_manager
