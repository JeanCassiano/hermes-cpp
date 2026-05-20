#include "../include/httplib.h"
#include "../include/json.hpp"
#include "../include/thread_pool.hpp"
#include <iostream>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <vector>

using json = nlohmann::json;

struct Event {
    std::string type;
    std::string email;
};

void process_event(const Event& e) {
    std::cout << "[WORKER] Sending '" << e.type
              << "' notification to " << e.email << "\n";
}

int main() {
    ThreadPool pool(4);
    httplib::Server server;

    server.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("ok", "text/plain");
    });

    server.Post("/notify", [&](const httplib::Request& req, httplib::Response& res) {
        auto body = json::parse(req.body);

        Event e;
        e.type  = body["type"];
        e.email = body["email"];

        pool.enqueue([e]() {
            process_event(e);
        });

        json response = {{"status", "queued"}, {"event", e.type}};
        res.set_content(response.dump(), "application/json");
    });

    std::cout << "[SERVER] Running on http://localhost:8080\n";
    server.listen("0.0.0.0", 8080);
}