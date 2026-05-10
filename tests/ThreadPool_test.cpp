# include <gtest/gtest.h>
# include "../include/thread_pool.hpp"


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
