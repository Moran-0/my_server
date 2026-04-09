#include "Channel.h"
#include "EventLoop.h"

Channel::Channel(EventLoop *_loop, int _fd) : loop(_loop), fd(_fd), events(0), inEpoll(false)
{
}

Channel::~Channel()
{
}

void Channel::handleEvent(uint32_t ready)
{
    if (ready & (EPOLLIN | EPOLLPRI))
    {
        if (readCallback)
        {
            readCallback();
        }
    }
    if (ready & EPOLLOUT)
    {
        if (writeCallback)
        {
            writeCallback();
        }
    }
}

void Channel::enableRead()
{
    events |= EPOLLIN | EPOLLPRI; // 可读事件以及紧急数据可读事件
    loop->updateChannel(this);
}

void Channel::disableAll()
{
    events = 0;
}
/// @brief 启用边缘触发模式
void Channel::useET()
{
    events |= EPOLLET;
    loop->updateChannel(this);
}

uint32_t Channel::getEvents()
{
    return events;
}

bool Channel::getInEpoll()
{
    return inEpoll;
}

void Channel::setInEpoll(bool _in)
{
    inEpoll = _in;
}

void Channel::setReadCallback(std::function<void()> cb)
{
    readCallback = cb;
}
