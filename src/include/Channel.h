#pragma once
#include <atomic>
#include <sys/epoll.h>
#include <functional>
#include "Common.h"

class Epoll;
class EventLoop;
class Channel : public NoCopy, NoMove {
  private:
    using CallbackFunc = std::function<void()>;
    EventLoop* m_loop;
    int m_fd;
    uint32_t m_events;
    bool m_inEpoll;
    CallbackFunc m_readCallback;
    CallbackFunc m_writeCallback;
    CallbackFunc m_connCallback;

  public:
    Channel(EventLoop* _loop, int _fd);
    ~Channel() = default;
    void handleEvent(uint32_t ready);
    void enableRead();
    void enableWrite();
    void setEvents(uint32_t ev);
    void disableAll();
    void useET();
    inline int getFd() {
        return m_fd;
    }
    uint32_t getEvents() const;
    bool getInEpoll() const;
    void setInEpoll(bool _in = true);
    void setReadCallback(CallbackFunc);
    void setWriteCallback(CallbackFunc);
    void setConnCallback(CallbackFunc);
};
