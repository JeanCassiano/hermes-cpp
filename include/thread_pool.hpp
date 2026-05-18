#pragma once
#include <iostream>
#include <functional>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <future>
#include <memory>
#include <type_traits>

class ThreadPool {
public:
    explicit    ThreadPool(size_t num_threads);
    ~ThreadPool();

    /**
     * @brief Enqueues a new task to be executed by the thread pool.
     * 
     * Safely pushes a new task into the queue and notifies one of the 
     * waiting worker threads to wake up and process it.
     * 
     * @param task A std::function representing the work to be executed.
     */
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) 
        -> std::future<typename std::invoke_result<F, Args...>::type> 
    {
        using return_type = typename std::invoke_result<F, Args...>::type;

        auto task = std::make_shared<std::packaged_task<return_type()>>(
        [f = std::forward<F>(f), ...args = std::forward<Args>(args)]() mutable {
            return std::invoke(f, std::move(args)...);
        }
        );
            
        std::future<return_type> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(this->queue_mutex);

            if(stop)
                throw std::runtime_error("enqueue on a stopped threadpool");

            tasks.push([task](){ (*task)(); });
        }
        condition.notify_one();
        return res;
    }
    size_t      get_worker_count();
    size_t      get_busy_worker_count();
    void        wait();
private:
    void worker_loop();
    std::atomic<size_t> busy_workers{0};
    std::vector<std::thread>            workers;
    std::queue<std::function<void()>>   tasks;
    std::mutex                          queue_mutex;
    std::condition_variable             condition;
    std::condition_variable             wait_condition;
    bool                                stop = false;
};