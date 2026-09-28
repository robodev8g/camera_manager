#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>

namespace camera_manager {

class MainThreadTaskQueue {
public:
    using Task = std::function<void()>;

    void post(Task task);
    void request_stop();
    void run();

private:
    std::mutex mutex_;
    std::condition_variable task_available_;
    std::queue<Task> tasks_;
};

}  // namespace camera_manager
