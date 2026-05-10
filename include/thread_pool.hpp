#pragma once
#include <iostream>
#include <functional>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>


class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads);
    ~ThreadPool();
    void enqueue(std::function<void()> task);
    size_t get_worker_count();
    size_t get_busy_worker_count();
private:
    void worker_loop();
    std::atomic<size_t> busy_workers{0};
    std::vector<std::thread>            workers;
    std::queue<std::function<void()>>   tasks;
    std::mutex                          queue_mutex;
    std::condition_variable             condition;
    bool                                stop = false;
};


