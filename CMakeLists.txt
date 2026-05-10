cmake_minimum_required(VERSION 3.14)

# Project name (this will be used in generated build files)
project(ThreadPoolTests)

# Set the C++ standard to C++17
# This ensures modern C++ features are available
set(CMAKE_CXX_STANDARD 17)

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

# ----------------------------
# Build test executable
# ----------------------------

# Create an executable called "run_tests"
# It includes:
#   - your test file
#   - your ThreadPool implementation file
add_executable(run_tests 
    tests/ThreadPool_test.cpp
    src/thread_pool.cpp
)

# ----------------------------
# Link libraries
# ----------------------------

# Link GoogleTest main (provides main() for tests)
# Link pthread because ThreadPool uses std::thread (Linux requirement)
target_link_libraries(run_tests gtest_main pthread)