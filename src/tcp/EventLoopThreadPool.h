#pragma once
#include "Common.h"
#include "EventLoopThread.h"
#include <vector>
#include <memory>

class EventLoop;
class EventLoopThreadPool : public NoCopy, public NoMove {
  public:
    EventLoopThreadPool(EventLoop* mainLoop, int numThreads = 0);
    ~EventLoopThreadPool() = default;
    void Start();
    EventLoop* GetNextLoop();

  private:
    EventLoop* m_mainLoop;
    std::vector<std::unique_ptr<EventLoopThread>> m_threads;
    std::vector<EventLoop*> m_loops;
    int m_threadNum;
    int m_next;
};