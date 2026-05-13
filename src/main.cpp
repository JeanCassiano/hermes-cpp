#include <iostream>
#include <chrono>
#include <mutex>
#include "../include/thread_pool.hpp" 

// We create a specific lock to organize terminal messages.
// Without this, threads would output text at the same time, scrambling the characters.
std::mutex print_mutex;

/**
 * @brief Simulates a time-consuming task.
 * 
 * This function puts the executing thread to sleep for a short duration 
 * to simulate processing time, and then safely prints a success message 
 * to the standard output.
 * 
 * @param task_id An integer identifier for the task being executed.
 */
void simulate_work(int task_id) {
    // 1. Put the thread to sleep for 100 milliseconds to simulate work
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 2. Lock the print mutex to safely output the message to the terminal
    std::unique_lock<std::mutex> lock(print_mutex);
    std::cout << "Task " << task_id << " was successfully completed by thread " 
              << std::this_thread::get_id() << "!\n";
}

int main() {
    std::cout << "--- Starting Thread Pool Test ---\n\n";

    // 1. Initialize the Thread Pool with 4 worker threads
    std::cout << "[MAIN] Creating a Thread Pool with 4 workers...\n";
    ThreadPool pool(4);

    // 2. Enqueue 10 tasks for execution
    std::cout << "[MAIN] Enqueuing 10 tasks to the execution queue...\n";
    for (int i = 1; i <= 10; ++i) {
        // We use a Lambda function [i]() to capture and pass the task number into the pool
        pool.enqueue([i]() {
            simulate_work(i);
        });
    }

    std::cout << "[MAIN] All tasks have been enqueued.\n";
    std::cout << "[MAIN] Calling wait() to block until all tasks are finished...\n\n";

    // 3. The main program is blocked here until the 10 tasks finish
    pool.wait();

    // 4. This line will only execute when the queue is empty and all threads are idle
    std::cout << "\n[MAIN] Success! All tasks are finished. Exiting program.\n";

    return 0;
}