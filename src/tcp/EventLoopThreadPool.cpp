#include "EventLoopThreadPool.h"

EventLoopThreadPool::EventLoopThreadPool(EventLoop* mainLoop, int numThreads):m_mainLoop(mainLoop),m_threadNum(numThreads),m_next(0) {}

void EventLoopThreadPool::Start() {
    for(int i = 0; i < m_threadNum; ++i) {
        std::unique_ptr<EventLoopThread> loopThread = std::make_unique<EventLoopThread>();
        m_threads.push_back(std::move(loopThread));
        m_loops.push_back(m_threads.back()->StartLoop());
    }
}

EventLoop* EventLoopThreadPool::GetNextLoop() {
    EventLoop* loop = m_mainLoop;
    // 轮询分配线程池中的EventLoop
    if(!m_loops.empty()) {
        loop = m_loops.at(m_next);
        ++m_next;
        m_next = m_next % m_threadNum;
    }
    return loop;
}
