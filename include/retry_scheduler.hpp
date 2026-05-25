#pragma once
#include <queue>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include "event.hpp"


class PriorityEventQueue;

class RetryScheduler
{
private:
    using TimePoint = std::chrono::system_clock::time_point;
    using ScheduledEvent = std::pair<TimePoint, Event>;

    struct MinHeapCmp {
        bool operator()(const ScheduledEvent& a, const ScheduledEvent& b){
            return a.first > b.first;
        }
    };

    void run();

    PriorityEventQueue& queue_;
    std::priority_queue<ScheduledEvent,
                        std::vector<ScheduledEvent>,
                        MinHeapCmp>     heap_;
    std::mutex              mutex_;
    std::condition_variable cv_;
    std::thread             thread_;
    bool                    stopped_ = false;
public:
    explicit RetryScheduler(PriorityEventQueue& queue);
    ~RetryScheduler();

    void schedule(Event event, int delay_seconds);
};
