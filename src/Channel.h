#pragma once
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
    uint32_t revents;
    bool inEpoll;
    std::function<void()> callback;

public:
    Channel(EventLoop *_loop, int _fd);
    ~Channel();
    void handleEvent();
    void EnableReading();
    inline int getFd()
    {
        return fd;
    }
    uint32_t getEvents();
    uint32_t getREvents();
    bool getInEpoll();
    void setInEpoll();
    void setRevents(uint32_t rev);
    void setCallback(std::function<void()>);
};