#pragma once
#include <string>
#include <chrono>

enum class Priority { CRITICAL = 0, HIGH = 1, NORMAL = 2};


struct Event {
    int id = 0;
    std::string type;
    std::string email;
    Priority priority = Priority::NORMAL;
    int retry_count = 0;
    int max_retries = 3;
    std::string last_error;
    std::chrono::system_clock::time_point next_retry_at;
    std::chrono::system_clock::time_point created_at;
};



struct EventComparator {
    bool operator()(const Event& a, const Event& b) const {
        if (a.priority != b.priority)
            return static_cast<int>(a.priority) > static_cast<int>(b.priority);
        return a.created_at > b.created_at;
    }
};