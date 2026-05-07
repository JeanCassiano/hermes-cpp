# include "../include/thread_pool.hpp"

ThreadPool::ThreadPool(size_t num_threads){
    for (size_t i = 0 ; i < num_threads; i++){
        workers.emplace_back(&ThreadPool::worker_loop, this);
    }
}

void ThreadPool::enqueue(std::function<void()> task){

}