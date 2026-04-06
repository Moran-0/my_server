#include "ThreadPool.h"
#include <exception>
#include <stdexcept>
#include <iostream>

ThreadPool::ThreadPool(int size) : stop(false)
{
    // int size = std::thread::hardware_concurrency();
    for (int i = 0; i < size; ++i)
    {
        threads.emplace_back(std::thread([this]()
                                         {
            while(true){
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(mtx);//出代码块自动解锁
                    cv.wait(lock, [this](){
                        return stop || !tasks.empty();
                    });
                    if(stop && tasks.empty()){
                        return; // 线程池终止并且没有剩余任务
                    }
                    task = tasks.front();
                    tasks.pop();
                }
                std::cout << "Current thread id:" << std::this_thread::get_id() << std::endl;;
                task();
            } }));
    }
}

ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> lock(mtx);
        stop = true;
    }
    cv.notify_all();
    for (auto &t : threads)
    {
        if (t.joinable())
        {
            t.join();
        }
    }
}
