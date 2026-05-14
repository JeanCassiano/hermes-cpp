cmake_minimum_required(VERSION 3.14)

# Project name (this will be used in generated build files)
project(ThreadPoolTests)

# Set the C++ standard to C++20
# This ensures modern C++ features are available
set(CMAKE_CXX_STANDARD 20)

# ----------------------------
# GoogleTest setup (FetchContent)
# ----------------------------

include(FetchContent)

# Download GoogleTest from GitHub at configure time
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)

# Force GoogleTest to use shared runtime (important on Windows, harmless on Linux)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Make GoogleTest available to the project
FetchContent_MakeAvailable(googletest)

# ==============================================================================
# TARGET 1: Google Test Executable
# ==============================================================================

# Create an executable called "run_tests"
add_executable(run_tests 
    tests/ThreadPool_test.cpp
    src/thread_pool.cpp
)

# Link GoogleTest main (provides main() for tests)
# Link pthread because ThreadPool uses std::thread (Linux requirement)
target_link_libraries(run_tests gtest_main pthread)

# ==============================================================================
# TARGET 2: Manual Test Executable (main.cpp)
# ==============================================================================

# Create a separate executable called "run_manual_test"
# We do this separately because main.cpp has its own main() function!
add_executable(run_manual_test 
    src/main.cpp
    src/thread_pool.cpp
)

# Link pthread because ThreadPool uses std::thread
# Note: We DO NOT link gtest_main here!
target_link_libraries(run_manual_test pthread)