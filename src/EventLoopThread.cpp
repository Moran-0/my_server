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

EventLoop* EventLoopThread::startLoop() {
    m_thread = std::thread(std::bind(&EventLoopThread::threadFunc, this));
    {
        std::unique_lock<std::mutex> lock(m_mtx);
        while (m_loop == nullptr) {
            m_cv.wait(lock);
        }
    }
    return m_loop;
}

void EventLoopThread::threadFunc() {
    EventLoop loop;
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_loop = &loop;
        m_cv.notify_one();
    }
    m_loop->loop();
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_loop = nullptr;
    }
}
