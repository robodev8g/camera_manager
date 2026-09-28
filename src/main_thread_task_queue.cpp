#include "main_thread_task_queue.hpp"

#include <utility>

namespace camera_manager {

void MainThreadTaskQueue::post(Task task) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        tasks_.push(std::move(task));
    }
    task_available_.notify_one();
}

void MainThreadTaskQueue::request_stop() {
    post({});
}

void MainThreadTaskQueue::run() {
    while (true) {
        Task task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            task_available_.wait(lock, [this] { return !tasks_.empty(); });
            task = std::move(tasks_.front());
            tasks_.pop();
        }

        if (!task) {
            return;
        }
        task();
    }
}

}  // namespace camera_manager
