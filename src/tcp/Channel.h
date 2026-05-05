#pragma once
#include <atomic>
#include <sys/epoll.h>
#include <functional>
#include <memory>
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
    bool m_tied;
    std::weak_ptr<void> m_tie;

  public:
    Channel(EventLoop* _loop, int _fd);
    ~Channel() = default;
    void HandleEvent(uint32_t ready);
    void HandleEventWithGuard(uint32_t ready);
    void EnableRead();
    void EnableWrite();
    void SetEvents(uint32_t ev);
    void DisableWrite();
    void DisableAll();
    void UseET();
    inline int GetFd() {
        return m_fd;
    }
    uint32_t GetEvents() const;
    bool GetInEpoll() const;
    void SetInEpoll(bool _in = true);
    void SetReadCallback(CallbackFunc);
    void SetWriteCallback(CallbackFunc);
    void SetConnCallback(CallbackFunc);

    // 设定tie关系，防止Channel被手动remove掉后，EventLoop还继续调用Channel的回调函数
    void Tie(const std::shared_ptr<void>& obj) {
        m_tie = obj;
        m_tied = true;
    }
};
