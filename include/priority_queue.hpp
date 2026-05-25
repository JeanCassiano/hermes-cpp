#pragma once
#include <queue>
#include <vector>
#include <mutex>
#include <condition_variable>
#include "event.hpp"


class PriorityEventQueue {
public:
    void push(Event e){
        {
            std::unique_lock<std::mutex> lock(mutex_);
            pq_.push(std::move(e));
        }
        cv_.notify_one();
    }

    Event pop(){
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock,  [this] { return !pq_.empty() || stopped_;});
        Event e = pq_.top();
        pq_.pop();
        return e;
    }

    bool try_pop(Event& out){
        std::unique_lock<std::mutex> lock(mutex_);
        if (pq_.empty()) return false;
        out = pq_.top();
        pq_.pop();
        return true;
    }

    void stop(){
        std::unique_lock<std::mutex> lock(mutex_);
        stopped_ = true;
        cv_.notify_all();
    }

    size_t size(){
        std::unique_lock<std::mutex> lock(mutex_);
        return pq_.size();
    }
private:
    std::priority_queue<Event, std::vector<Event>, EventComparator> pq_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stopped_ = false;
};