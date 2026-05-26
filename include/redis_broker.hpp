#pragma once
#include <hiredis/hiredis.h>
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <chrono>
#include "event.hpp"
#include "logger.hpp"

class RedisBroker {
public:
    explicit RedisBroker(const std::string& host, int port)
        : host_(host), port_(port) {
        pub_ctx_ = redisConnect(host.c_str(), port);
        if (!pub_ctx_ || pub_ctx_->err) {
            Logger::error("Failed to connect to Redis at " + host + ":" + std::to_string(port));
        }
    }

    ~RedisBroker() {
        stop();
        if (sub_thread_.joinable()) sub_thread_.join();
        if (pub_ctx_) redisFree(pub_ctx_);
        if (sub_ctx_) redisFree(sub_ctx_);
    }

    bool publish(const std::string& channel, const std::string& payload) {
        if (!pub_ctx_ || pub_ctx_->err) {
            return false;
        }
        redisReply* r = (redisReply*)redisCommand(pub_ctx_, "PUBLISH %s %s",
                                                   channel.c_str(), payload.c_str());
        if (!r) {
            return false;
        }
        freeReplyObject(r);
        return true;
    }

    void subscribe(const std::string& channel,
                   std::function<void(const std::string&)> cb) {
        sub_ctx_ = redisConnect(host_.c_str(), port_);
        if (!sub_ctx_ || sub_ctx_->err) {
            Logger::error("Failed to connect Redis subscriber");
            return;
        }

        channel_ = channel;
        callback_ = cb;
        sub_thread_ = std::thread([this] { run_subscriber(); });
    }

    void stop() { running_ = false; }

private:
    void run_subscriber() {
        redisReply* reply = nullptr;

        // SUBSCRIBE to the channel
        reply = (redisReply*)redisCommand(sub_ctx_, "SUBSCRIBE %s", channel_.c_str());
        if (reply) freeReplyObject(reply);

        // Set a small read timeout to allow checking running_ flag
        struct timeval tv{0, 100000}; // 100ms timeout
        redisSetReadTimeout(sub_ctx_, &tv);

        while (running_) {
            reply = nullptr;
            int status = redisGetReply(sub_ctx_, (void**)&reply);

            if (status == REDIS_OK && reply) {
                // Subscribe message is an array: ["message", channel, data]
                if (reply->type == REDIS_REPLY_ARRAY && reply->elements == 3) {
                    if (reply->element[0]->str &&
                        std::string(reply->element[0]->str) == "message" &&
                        reply->element[2]->str) {
                        std::string payload(reply->element[2]->str);
                        if (payload == "__stop__") {
                            freeReplyObject(reply);
                            break; // Graceful shutdown signal
                        }
                        callback_(payload);
                    }
                }
                freeReplyObject(reply);
            } else if (status == REDIS_ERR) {
                Logger::warn("Redis subscriber error");
                break;
            }
            // On timeout (no reply), loop continues to check running_ flag
        }

        // Unsubscribe
        reply = (redisReply*)redisCommand(sub_ctx_, "UNSUBSCRIBE");
        if (reply) freeReplyObject(reply);

        Logger::info("Redis subscriber stopped");
    }

    std::string host_;
    int port_;
    redisContext* pub_ctx_ = nullptr;
    redisContext* sub_ctx_ = nullptr;
    std::string channel_;
    std::function<void(const std::string&)> callback_;
    std::thread sub_thread_;
    std::atomic<bool> running_{true};
};
