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
    std::function<void()> readCallback;
    std::function<void()> writeCallback;

public:
    Channel(EventLoop *_loop, int _fd);
    ~Channel() = default;
    void handleEvent(uint32_t ready);
    void enableRead();
    void disableAll();
    void useET();
    inline int getFd()
    {
        return fd;
    }
    uint32_t getEvents() const;
    bool getInEpoll() const;
    void setInEpoll(bool _in = true);
    void setReadCallback(std::function<void()>);
};
