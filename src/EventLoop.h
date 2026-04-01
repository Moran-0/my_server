#pragma once
class Epoll;
class Channel;
class ThreadPool;

class EventLoop
{
private:
    Epoll *ep;
    bool quit;
    ThreadPool *thread_pool;

public:
    EventLoop();
    ~EventLoop();
    void loop();
    void updateChannel(Channel *ch);
};