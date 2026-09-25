#pragma once
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace shn::world {
struct UploadRequest { std::uint64_t assetId{}; std::string source; std::vector<std::uint8_t> bytes; };

class StreamingUploadQueue {
    std::queue<UploadRequest> queue_;
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    bool stopping_{};
public:
    void push(UploadRequest request) { { std::lock_guard lock(mutex_); if (stopping_) return; queue_.push(std::move(request)); } condition_.notify_one(); }
    bool pop(UploadRequest& out) {
        std::unique_lock lock(mutex_);
        condition_.wait(lock,[this]{return stopping_ || !queue_.empty();});
        if (queue_.empty()) return false;
        out=std::move(queue_.front()); queue_.pop(); return true;
    }
    void stop() { { std::lock_guard lock(mutex_); stopping_=true; } condition_.notify_all(); }
    std::size_t pending() const { std::lock_guard lock(mutex_); return queue_.size(); }
};
}
