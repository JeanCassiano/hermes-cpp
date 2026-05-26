#pragma once
#include <atomic>
#include <string>
#include "../include/json.hpp"

using json = nlohmann::json;

struct Metrics {
    std::atomic<int> sent{0};
    std::atomic<int> failed{0};
    std::atomic<int> dlq_total{0};
    std::atomic<int> rate_limit_hits{0};
    std::atomic<long long> total_latency_ms{0};
    std::string tag;  // "internal" or "redis"

    void record_sent(long long latency_ms) {
        ++sent;
        total_latency_ms += latency_ms;
    }

    json snapshot() const {
        int s = sent.load();
        return {
            {"sent",             s},
            {"failed",           failed.load()},
            {"dlq_total",        dlq_total.load()},
            {"rate_limit_hits",  rate_limit_hits.load()},
            {"avg_latency_ms",   s > 0 ? total_latency_ms.load() / s : 0}
        };
    }
};
