#include "../include/thread_pool.hpp"
#include <stdexcept>
/**
 * @brief Constructs a ThreadPool with a specified number of worker threads.
 * 
 * Initializes the pool and starts the worker threads. Each thread immediately
 * begins executing the worker_loop, waiting for tasks to be enqueued.
 * 
 * @param num_threads The number of worker threads to spawn in the pool.
 * @throws std::invalid_argument if num_threads is 0.
 */
ThreadPool::ThreadPool(size_t num_threads) : stop(false), busy_workers(0) {
    if (num_threads == 0) {
        throw std::invalid_argument("ThreadPool must have at least 1 worker thread.");
    }

    for (size_t i = 0 ; i < num_threads; i++) {
        workers.emplace_back(&ThreadPool::worker_loop, this);
    }
}

/**
 * @brief The main execution loop for each worker thread.
 * 
 * Threads run in an infinite loop, waiting on a condition variable. 
 * When awakened, a thread safely dequeues a task, marks itself as busy, 
 * and attempts to execute it. A try-catch block guarantees that exceptions 
 * inside tasks do not terminate the worker thread (Resilience).
 */
void ThreadPool::worker_loop() {
    while(true) {
        std::function<void()> task;
        
        { 
            std::unique_lock<std::mutex> lock(this->queue_mutex);
            this->condition.wait(lock, [this] {
                return this->stop || !this->tasks.empty();
            });

            if (this->stop && this->tasks.empty()) {
                return;
            }

            task = std::move(this->tasks.front());
            this->tasks.pop();
        }

        this->busy_workers++;
        
        try {
            task(); 
        } 
        catch (const std::exception& e) {
            std::cerr << "Task error: " << e.what() << '\n';
        } 
        catch (...) {
            std::cerr << "Unknown task error occurred.\n";
        }
        

        this->busy_workers--;

        {
            std::unique_lock<std::mutex> lock(this->queue_mutex);
            if (this->tasks.empty() && this->busy_workers.load() == 0) {
                this->wait_condition.notify_all();
            }
        }
    }
}

/**
 * @brief Destructs the ThreadPool and cleanly shuts down all worker threads.
 * 
 * Sets the stop flag to true and notifies all sleeping threads. It then 
 * waits for all threads to finish their current execution and terminate 
 * via join() before finally destroying the ThreadPool object.
 */
ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(this->queue_mutex);
        this->stop = true;
    }

    this->condition.notify_all();

    for (std::thread &worker : this->workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

/**
 * @brief Blocks the calling thread until all tasks are completed.
 * 
 * This function acquires the lock and waits on the wait_condition.
 * It will only wake up and return when the task queue is entirely empty 
 * AND no worker threads are currently busy executing tasks.
 */
void ThreadPool::wait() {
    std::unique_lock<std::mutex> lock(this->queue_mutex);    
    this->wait_condition.wait(lock, [this] {
        return this->tasks.empty() && (this->busy_workers.load() == 0);
    });
}

/**
 * @brief Retrieves the total number of worker threads in the pool.
 * 
 * @return size_t The total capacity of threads in the pool.
 */
size_t ThreadPool::get_worker_count() {
    return this->workers.size();
}

/**
 * @brief Retrieves the number of threads currently executing a task.
 * 
 * This uses the atomic busy_workers counter to safely check how many 
 * threads are actively processing tasks at the moment of the call.
 * 
 * @return size_t The count of busy threads.
 */
size_t ThreadPool::get_busy_worker_count() {
    return this->busy_workers.load();
}

