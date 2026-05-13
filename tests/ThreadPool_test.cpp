/*
    TEST STRUCTURE:

    TEST(TestSuiteName, TestName) {
        // Arrange   -> prepare the data/objects
        // Act       -> execute the behavior
        // Assert    -> verify the result
    }
*/

#include <gtest/gtest.h>          // Imports the Google Test framework
#include "../include/thread_pool.hpp" // Imports your ThreadPool class
#include <atomic>
/*
    TEST STRUCTURE:

    TEST(TestSuiteName, TestName) {
        // Arrange   -> prepare the data/objects
        // Act       -> execute the behavior
        // Assert    -> verify the result
    }

    This pattern is extremely common in unit testing.
*/

/*
    HOW TO THINK WHEN WRITING TESTS:

    1. Ask yourself:
       "What behavior am I validating?"

       Examples:
       - Constructor accepts valid values
       - Constructor rejects invalid values
       - Tasks are executed correctly
       - Threads shut down properly

    2. Separate tests by responsibility.
       One behavior = one test.

    3. Use descriptive names.
       Good test names explain:
       - What is being tested
       - Expected behavior

       Example:
       TEST(ThreadPoolTest, ExecutesSubmittedTasks)

    4. Prefer testing behavior instead of implementation.
       Don't test private variables directly.
       Test observable results.

    5. Common Google Test assertions:

       EXPECT_EQ(a, b)        // checks equality
       EXPECT_NE(a, b)        // checks inequality
       EXPECT_TRUE(value)     // checks if true
       EXPECT_FALSE(value)    // checks if false
       EXPECT_THROW(code, ExceptionType)
       EXPECT_NO_THROW(code)

    6. Typical testing mindset:
       - Valid input
       - Invalid input
       - Edge cases
       - Concurrency behavior
       - Resource cleanup
*/

// This test verifies if the constructor accepts a size_t parameter
TEST(ThreadPoolTest, AcceptParameterSizeT) {

    // ---------------- ARRANGE ----------------
    // Create the input value we want to test.
    // size_t is commonly used for sizes/counts in C++.
    size_t number_of_threads = 5;

    
    // ---------------- ACT + ASSERT ----------------
    // EXPECT_NO_THROW checks if the code inside the block
    // executes WITHOUT throwing exceptions.
    //
    // If the constructor throws an exception,
    // this test will fail automatically.
    EXPECT_NO_THROW({

        // Create the ThreadPool object using the parameter.
        // If construction succeeds, the test passes.
        ThreadPool pool(number_of_threads);
    });
}

// This test verifies if we create exactly N worker threads in the constructor
TEST(ThreadPoolTest, CreateExactlyNWorkers){
    size_t number_of_threads = 5;
    ThreadPool pool(number_of_threads);
    EXPECT_EQ(pool.get_worker_count(), number_of_threads);
}

// Test if each worker starts in a idle state
TEST(ThreadPoolTest, WorkersStartIdle){
    size_t number_of_threads = 5;
    ThreadPool pool(number_of_threads);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_EQ(pool.get_busy_worker_count(), 0);
}

// This test verifies if the constructor rejects invalid arguments (0)
TEST(ThreadPoolTest, RejectZeroThreads){
    size_t invalid_number = 0;
    EXPECT_THROW(ThreadPool pool(invalid_number), std::invalid_argument);
}

// ==========================================
// F2 — Task Enqueueing & F3 — Parallel Execution
// ==========================================

// Validates F2.1, F2.2 and F3.1: Accepts valid tasks, executes them, and does so in parallel
TEST(ThreadPoolTest, ExecutesTasksInParallel) {
    // Arrange
    ThreadPool pool(4);
    std::atomic<int> completed_tasks{0};
    int total_tasks = 10;

    // Act
    for(int i = 0; i < total_tasks; i++) {
        pool.enqueue([&completed_tasks]() {
            // Small delay to ensure parallel execution happens
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            completed_tasks++;
        });
    }
    
    // Wait for all tasks to finish
    pool.wait();

    // Assert
    EXPECT_EQ(completed_tasks.load(), total_tasks);
}

// Validates F3.2: Excess tasks remain queued until a worker becomes available
TEST(ThreadPoolTest, QueuesExcessTasks) {
    // Arrange: Only 2 workers
    ThreadPool pool(2);
    std::atomic<int> active_tasks{0};
    std::mutex m;
    std::condition_variable cv;
    bool block_tasks = true;

    // Act: Send 5 tasks (3 more than the workers can handle at once)
    for(int i = 0; i < 5; i++) {
        pool.enqueue([&]() {
            active_tasks++;
            std::unique_lock<std::mutex> lock(m);
            // Tasks freeze here until block_tasks becomes false
            cv.wait(lock, [&]() { return !block_tasks; });
        });
    }

    // Wait a bit to let the first 2 tasks start
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Assert: Only 2 tasks should be running. The other 3 are waiting in the queue.
    EXPECT_EQ(active_tasks.load(), 2);
    EXPECT_EQ(pool.get_busy_worker_count(), 2);

    // Cleanup: Release all frozen tasks so the test can finish
    {
        std::unique_lock<std::mutex> lock(m);
        block_tasks = false;
    }
    cv.notify_all();
    pool.wait();
}

// ==========================================
// F4 — Graceful Shutdown
// ==========================================

// Validates F4.1, F4.2, F4.3, F4.4: Destructor finishes pending tasks and joins threads
TEST(ThreadPoolTest, GracefulShutdownCompletesAllTasks) {
    std::atomic<int> completed_tasks{0};
    int total_tasks = 5;

    // Arrange & Act
    {
        // We create the pool inside a local scope
        ThreadPool pool(3);
        for(int i = 0; i < total_tasks; i++) {
            pool.enqueue([&completed_tasks]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                completed_tasks++;
            });
        }
    } // The scope ends here! Destructor is called AUTOMATICALLY.

    // Assert
    // If the destructor is implemented correctly, it will wait for all tasks 
    // to finish before destroying the object and continuing execution.
    EXPECT_EQ(completed_tasks.load(), total_tasks);
}

// ==========================================
// F6 — Resilience
// ==========================================

// Validates F6.1, F6.2, F6.3: Thread pool recovers from exceptions inside tasks
TEST(ThreadPoolTest, RecoversFromTaskExceptions) {
    // Arrange
    ThreadPool pool(2);
    std::atomic<int> successful_tasks{0};

    // Act
    // 1. Send a task that will purposefully crash (throw an exception)
    pool.enqueue([]() {
        throw std::runtime_error("Simulated task failure");
    });

    // Let the crash happen
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    // 2. Send a normal task. If the worker thread died, this might not execute.
    pool.enqueue([&successful_tasks]() {
        successful_tasks++;
    });

    pool.wait();

    // Assert
    // The pool should have survived the exception and completed the second task.
    EXPECT_EQ(successful_tasks.load(), 1);
}