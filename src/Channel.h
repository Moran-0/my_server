#pragma once
#include <atomic>
#include <sys/epoll.h>
#include <functional>

class Epoll;
class EventLoop;
class Channel
{
private:
    // Epoll *ep;
    EventLoop *loop;
    int fd;
    uint32_t events;
    bool inEpoll;
    bool useThreadPool;         // 默认使用线程池
    std::atomic<bool> handling; // 原子类型，标记处理状态，防止竞态发生
    std::function<void()> readCallback;
    std::function<void()> writeCallback;

public:
    Channel(EventLoop *_loop, int _fd);
    ~Channel();
    void handleEvent(uint32_t ready);
    void enableRead();
    void disableAll();
    void useET();
    inline int getFd()
    {
        return fd;
    }
    uint32_t getEvents();
    bool getInEpoll();
    void setInEpoll(bool _in = true);
    void setReadCallback(std::function<void()>);
    void setUseThreadPool(bool);
    bool tryStartHandling();
    void finishHandling();
    inline bool getUseThreadPool()
    {
        return useThreadPool;
    }
};
