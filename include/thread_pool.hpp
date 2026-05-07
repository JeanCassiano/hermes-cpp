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
private:
    void worker_loop();

    std::vector<std::thread>            workers;
    std::queue<std::function<void()>>   tasks;
    std::mutex                          queue_mutex;
    std::condition_variable             condition;
    bool                                stop = false;
};

