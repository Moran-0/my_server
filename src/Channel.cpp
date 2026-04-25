#include "Channel.h"
#include "EventLoop.h"

Channel::Channel(EventLoop* _loop, int _fd) : m_loop(_loop), m_fd(_fd), m_events(0), m_inEpoll(false) {}

void Channel::handleEvent(uint32_t ready)
{
    if (ready & (EPOLLIN | EPOLLPRI))
    {
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

void Channel::enableRead()
{
    m_events |= EPOLLIN | EPOLLPRI; // 可读事件以及紧急数据可读事件
    m_loop->updateChannel(this);
}

void Channel::enableWrite() {
    m_events |= EPOLLOUT; // 可写事件
    m_loop->updateChannel(this);
}

void Channel::setEvents(uint32_t ev) {
    m_events = ev;
}

void Channel::disableAll()
{
    m_events = 0;
}
/// @brief 启用边缘触发模式
void Channel::useET()
{
    m_events |= EPOLLET;
    m_loop->updateChannel(this);
}

uint32_t Channel::getEvents() const
{
    return m_events;
}

bool Channel::getInEpoll() const
{
    return m_inEpoll;
}

void Channel::setInEpoll(bool _in)
{
    m_inEpoll = _in;
}

void Channel::setReadCallback(CallbackFunc cb) {
    m_readCallback = std::move(cb);
}

void Channel::setWriteCallback(CallbackFunc cb) {
    m_writeCallback = std::move(cb);
}

void Channel::setConnCallback(CallbackFunc cb) {
    m_connCallback = std::move(cb);
}
