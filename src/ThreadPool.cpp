#include "ThreadPool.h"
#include <exception>
#include <stdexcept>
#include <iostream>

ThreadPool::ThreadPool(/* args */) : stop(false)
{
    int size = std::thread::hardware_concurrency();
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
}

void ThreadPool::addTask(std::function<void()> task)
{
    {
        std::lock_guard<std::mutex> lock(mtx);
        if (stop)
        {
            throw std::runtime_error("线程池已停止，无法添加任务！");
        }
        tasks.emplace(task);
    }
    cv.notify_one();
}
