#pragma once
class Epoll;
class Channel;
class ThreadPool;
#include <memory>
#include "Epoll.h"

class EventLoop
{
private:
  // Epoll *ep;
  std::unique_ptr<Epoll> ep;
  bool m_quit;
  // ThreadPool *thread_pool;

public:
    EventLoop();
    ~EventLoop() = default;
    void loop();
    void quit(); // todo
    void updateChannel(Channel *ch);
    void removeChannel(Channel *ch);
};
