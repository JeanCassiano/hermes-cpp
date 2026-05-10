# include "../include/thread_pool.hpp"

ThreadPool::ThreadPool(size_t num_threads){
    for (size_t i = 0 ; i < num_threads; i++){
        workers.emplace_back(&ThreadPool::worker_loop, this);
    }
}

void ThreadPool::worker_loop(){

}

ThreadPool::~ThreadPool(){

}

void ThreadPool::enqueue(std::function<void()> task){

}

size_t ThreadPool::get_worker_count(){
    return this->workers.size();
}

size_t ThreadPool::get_busy_worker_count(){
    return this->busy_workers.load();
}
