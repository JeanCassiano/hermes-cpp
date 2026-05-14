#include <iostream>
#include <chrono>
#include <mutex>
#include <string>
#include <vector>
#include <future>
#include <cstdlib> // For rand()
#include "../include/thread_pool.hpp" 

// We use a specific mutex to organize terminal output.
// This prevents threads from printing at the same time and scrambling the text.
std::mutex print_mutex;

/**
 * @brief Simulates a time-consuming task with a random duration.
 * 
 * @param task_id An integer identifier for the task being executed.
 * @return std::string A formatted message containing the execution details.
 */
std::string simulate_work(int task_id) {
    // 1. Define a random sleep time between 100ms and 600ms to simulate varying workloads
    int sleep_time = 100 + (std::rand() % 500);
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
    
    // 2. Assemble the final message
    // We grab the last 4 digits of the thread ID to make it readable in the terminal
    size_t thread_id_short = std::hash<std::thread::id>{}(std::this_thread::get_id()) % 10000;
    
    std::string result_msg = "Task " + std::to_string(task_id) + 
                             " completed in " + std::to_string(sleep_time) + "ms " +
                             "[Thread " + std::to_string(thread_id_short) + "]";

    // 3. Print IMMEDIATELY as soon as the task finishes!
    {
        std::unique_lock<std::mutex> lock(print_mutex);
        std::cout << "[WORKER] " << result_msg << "\n";
    }

    // Return the string to fulfill the std::future contract
    return result_msg;
}

int main() {
    // Initialize the random seed so we get different sleep times on every run
    std::srand(std::time(nullptr)); 

    std::cout << "--- Starting Real-Time Thread Pool Test ---\n\n";

    std::cout << "[MAIN] Creating a Thread Pool with 4 workers...\n";
    ThreadPool pool(4);

    std::vector<std::future<std::string>> results;

    std::cout << "[MAIN] Dispatching 10 tasks to the queue...\n";
    std::cout << "----------------------------------------------------\n";
    
    for (int i = 1; i <= 10; ++i) {
        results.emplace_back(
            pool.enqueue([i]() {
                return simulate_work(i);
            })
        );
    }

    std::cout << "[MAIN] All tasks enqueued. The main() thread will now wait for completion.\n\n";

    // 4. We call .get() just to ensure the main thread doesn't terminate before the workers.
    // Since the printing was already handled by the workers, we don't need to print here.
    for (size_t i = 0; i < results.size(); ++i) {
        results[i].get(); 
    }

    std::cout << "\n----------------------------------------------------\n";
    std::cout << "[MAIN] Success! All tasks finished. Shutting down server.\n";

    return 0;
}