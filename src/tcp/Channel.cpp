#include "Channel.h"
#include "EventLoop.h"

Channel::Channel(EventLoop* _loop, int _fd) : m_loop(_loop), m_fd(_fd), m_events(0), m_inEpoll(false), m_tied(false) {}

void Channel::HandleEvent(uint32_t ready) {
    if (m_tied) {
        std::shared_ptr<void> guard = m_tie.lock(); // 确保处理事件时，Channel对象还存在
        if (guard) {
            HandleEventWithGuard(ready);
        }
    } else {
        HandleEventWithGuard(ready);
    }
}

void Channel::HandleEventWithGuard(uint32_t ready) {
    if (ready & (EPOLLIN | EPOLLPRI)) {
        if (m_readCallback)
        {
            m_readCallback();
        }
    }
    if (ready & EPOLLOUT)
    {
        if (m_writeCallback)
        {
            m_writeCallback();
        }
    }
}

void Channel::EnableRead() {
    m_events |= EPOLLIN | EPOLLPRI; // 可读事件以及紧急数据可读事件
    m_loop->updateChannel(this);
}

void Channel::EnableWrite() {
    m_events |= EPOLLOUT; // 可写事件
    m_loop->updateChannel(this);
}

void Channel::SetEvents(uint32_t ev) {
    m_events = ev;
}

void Channel::DisableWrite() {
    if (m_events & EPOLLOUT) {
        m_events &= ~EPOLLOUT;
        m_loop->updateChannel(this);
    }
}

void Channel::DisableAll() {
    m_events = 0;
}
/// @brief 启用边缘触发模式
void Channel::UseET() {
    m_events |= EPOLLET;
    m_loop->updateChannel(this);
}

uint32_t Channel::GetEvents() const {
    return m_events;
}

bool Channel::GetInEpoll() const {
    return m_inEpoll;
}

void Channel::SetInEpoll(bool _in) {
    m_inEpoll = _in;
}

void Channel::SetReadCallback(CallbackFunc cb) {
    m_readCallback = std::move(cb);
}

void Channel::SetWriteCallback(CallbackFunc cb) {
    m_writeCallback = std::move(cb);
}

void Channel::SetConnCallback(CallbackFunc cb) {
    m_connCallback = std::move(cb);
}
