#include <iostream>
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include "../include/thread_pool.hpp"



struct Event {
    std::string type;
    std::string email;
};




void process_event(const Event& e){
    std::cout << "[WORKER] Sending '" << e.type 
              << "' notification to " << e.email << "\n";
}
void producer(ThreadPool& pool) {
    std::vector<Event> events = {
        {"user_registered",  "joao@usp.br"},
        {"order_confirmed",  "maria@usp.br"},
        {"password_reset",   "pedro@usp.br"},
    };

    for (const auto& event : events){
        pool.enqueue([event](){
            process_event(event);
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    
}

int main(){
    ThreadPool pool(4);

    std::thread p(producer, std::ref(pool));
    p.join();


    return 0;
}