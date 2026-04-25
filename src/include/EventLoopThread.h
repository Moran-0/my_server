#pragma once
#include <thread>
#include <mutex>
#include <condition_variable>

class EventLoop;
class EventLoopThread {
  public:
    EventLoopThread();
    ~EventLoopThread();
    EventLoop* startLoop();

  private:
    void threadFunc();

  private:
    EventLoop* m_loop;
    std::thread m_thread;
    bool exiting; // 线程退出状态（是否正在退出）
    std::mutex m_mtx;
    std::condition_variable m_cv;
};