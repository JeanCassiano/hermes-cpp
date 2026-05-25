#pragma once
#include <iostream>
#include <mutex>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>

class Logger {
public:
    static void info (const std::string& msg) {log("INFO", msg);}
    static void warn (const std::string& msg) { log("WARN ", msg); }
    static void error(const std::string& msg) { log("ERROR", msg); }
private:
    static void log(const char* level, const std::string& msg) {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);

        size_t tid = std::hash<std::thread::id>{}(std::this_thread::get_id()) % 10000;

        std::ostringstream ss;
        ss << "[" << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S") << "]"
           << " [" << level << "]"
           << " [thread:" << tid << "] "
           << msg;

        std::unique_lock<std::mutex> lock(mutex_);
        std::cout << ss.str() << "\n";

    }
    static inline std::mutex mutex_;
};