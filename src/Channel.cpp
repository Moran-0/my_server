#include "Channel.h"
#include "EventLoop.h"

Channel::Channel(EventLoop *_loop, int _fd) : loop(_loop), fd(_fd), events(0), revents(0), inEpoll(false)
{
}

Channel::~Channel()
{
}

void Channel::handleEvent()
{
    callback();
}

void Channel::EnableReading()
{
    events = EPOLLIN | EPOLLET;
    loop->updateChannel(this);
}

uint32_t Channel::getEvents()
{
    return events;
}

uint32_t Channel::getREvents()
{
    return revents;
}

bool Channel::getInEpoll()
{
    return inEpoll;
}

void Channel::setInEpoll()
{
    inEpoll = true;
}

void Channel::setRevents(uint32_t rev)
{
    revents = rev;
}

void Channel::setCallback(std::function<void()> cb)
{
    callback = cb;
}
