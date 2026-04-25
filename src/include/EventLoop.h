#pragma once
class Epoll;
class Channel;
class ThreadPool;
#include <memory>
#include <vector>
#include <mutex>
#include <functional>
#include "Epoll.h"
#include "CurrentThread.h"
#include "Channel.h"
class EventLoop
{
private:
  std::unique_ptr<Epoll> m_ep;
  bool m_quit;
  std::mutex m_mtx;
  std::vector<std::function<void()>> m_todoList;
  int m_wakeupFd;                           // 用于唤醒EventLoop的fd
  std::unique_ptr<Channel> m_wakeupChannel; // 用于监听m_wakeupFd的Channel
  bool m_callingTodoList;                   // 标志当前是否正在执行m_todoList中的任务
  pid_t m_threadId;                         // 记录创建EventLoop的线程ID

public:
    EventLoop();
    ~EventLoop() = default;
    void loop();
    void quit(); // todo
    void updateChannel(Channel *ch);
    void removeChannel(Channel *ch);

    void RunOneFunc(const std::function<void()>& func); // 在EventLoop所在的线程中执行func
    void QueueFunc(const std::function<void()>& func);  // 将func添加到m_todoList中，并唤醒EventLoop所在的线程

  private:
    void DoTodoList();
    void HandleRead() const; // 用于处理m_wakeupFd上的可读事件，即执行m_todoList中的任务
    bool IsInLoopThread() const {
        return CurrentThread::Tid() == m_threadId;
    }
};
