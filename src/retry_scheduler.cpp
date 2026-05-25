#include "../include/retry_scheduler.hpp"
#include "../include/priority_queue.hpp"
#include "../include/logger.hpp"

RetryScheduler::RetryScheduler(PriorityEventQueue& queue) : queue_(queue) {
    thread_ = std::thread([this] { run(); });
}

RetryScheduler::~RetryScheduler() {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        stopped_ = true;
    }
    cv_.notify_all();
    if (thread_.joinable()) thread_.join();
}

void RetryScheduler::schedule(Event event, int delay_seconds) {
    auto fire_at = std::chrono::system_clock::now()
                 + std::chrono::seconds(delay_seconds);
    event.next_retry_at = fire_at;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        heap_.push({fire_at, std::move(event)});
    }
    cv_.notify_one();
}

void RetryScheduler::run() {
    while (true) {
        std::unique_lock<std::mutex> lock(mutex_);

        if (heap_.empty()) {
            cv_.wait(lock, [this] { return !heap_.empty() || stopped_; });
        }

        if (stopped_) return;

        auto [fire_at, event] = heap_.top();
        auto now = std::chrono::system_clock::now();

        if (fire_at > now) {
            // dorme até o próximo evento (ou até alguém empurrar novo)
            cv_.wait_until(lock, fire_at);
            continue;
        }

        heap_.pop();
        lock.unlock();

        Logger::info("Retrying event " + std::to_string(event.id)
                     + " (attempt " + std::to_string(event.retry_count + 1) + ")");
        queue_.push(event);
    }
}
