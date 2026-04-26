#include "EventLoopThread.h"
#include "EventLoop.h"
#include <future>
#include <functional>

EventLoopThread::EventLoopThread() : m_loop(nullptr), exiting(false) {}

EventLoopThread::~EventLoopThread() {
    exiting = true;
    if (m_loop) {
        m_loop->quit();
        m_thread.join(); // 正确退出线程
    }
}

EventLoop* EventLoopThread::StartLoop() {
    // m_thread = std::thread(std::bind(&EventLoopThread::ThreadFunc, this));
    m_thread = std::thread([this]() { this->ThreadFunc(); });
    {
        std::unique_lock<std::mutex> lock(m_mtx);
        while (m_loop == nullptr) {
            m_cv.wait(lock); // 等待m_thread线程函数创建EventLoop并设置m_loop
        }
    }
    return m_loop;
}

void EventLoopThread::ThreadFunc() {
    EventLoop loop;
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_loop = &loop;
        m_cv.notify_one();
    }
    m_loop->loop(); // loop函数会一直运行，直到调用quit()退出循环，loop自动析构
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_loop = nullptr;
    }
}
