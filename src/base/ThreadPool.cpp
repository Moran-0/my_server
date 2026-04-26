#include "ThreadPool.h"
#include <exception>
#include <stdexcept>
#include <iostream>

ThreadPool::ThreadPool(int size) : m_stop(false) {
    if (size < 0) {
        throw std::invalid_argument("Thread pool size must be positive(>0)!");
    }
    // int size = std::thread::hardware_concurrency();
    for (int i = 0; i < size; ++i) {
        m_threads.emplace_back(std::thread([this]() {
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(m_mtx); // 出代码块自动解锁
                    m_cv.wait(lock, [this]() { return m_stop || !m_tasks.empty(); });
                    if (m_stop && m_tasks.empty()) {
                        return; // 线程池终止并且没有剩余任务
                    }
                    task = m_tasks.front();
                    m_tasks.pop();
                }
                std::cout << "Current thread id:" << std::this_thread::get_id() << '\n';
                task();
            }
        }));
    }
}

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(m_mtx);
        m_stop = true;
    }
    m_cv.notify_all();
    for (auto& t : m_threads) {
        if (t.joinable()) {
            t.join();
        }
    }
}
