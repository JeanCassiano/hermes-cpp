#pragma once
#include <atomic>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>

class RateLimiter {
public:
    RateLimiter(int rate_per_sec, int burst)
        : tokens_(burst), max_tokens_(burst),
        interval_(1000 /rate_per_sec)
    {
        refill_thread_ = std::thread([this]{ refill_loop();});
    }

    ~RateLimiter(){
        stopped_ = true;
        cv_.notify_all();
        if (refill_thread_.joinable())
            refill_thread_.join();
    }

    void acquire(){
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] {return tokens_ > 0 || stopped_;});
        --tokens_;
    }

    bool try_acquire(){
        std::unique_lock<std::mutex> lock(mutex_);
        if (tokens_ == 0) return false;
        --tokens_;
        return true;
    }

private:

    void refill_loop(){
        while (!stopped_){
            std::this_thread::sleep_for(std::chrono::milliseconds(interval_));
            {
                std::unique_lock<std::mutex> lock(mutex_);
                if (tokens_ < max_tokens_) ++tokens_;
            }
            cv_.notify_all();
        }
    }
    int tokens_;
    const int max_tokens_;
    const int interval_;
    std::atomic<bool> stopped_{false};
    std::thread refill_thread_;
    std::mutex mutex_;
    std::condition_variable cv_;
};