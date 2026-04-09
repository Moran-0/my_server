#include "EventLoop.h"
#include "Epoll.h"
#include "Channel.h"
#include "ThreadPool.h"
#include <iostream>

EventLoop::EventLoop() : ep(nullptr), quit(false)
{
    ep = new Epoll();
}

EventLoop::~EventLoop()
{
    delete ep;
    ep = nullptr;
}

void EventLoop::loop()
{
    while (!quit)
    {
        auto chs = ep->poll();
        for (auto &channel_event : chs)
        {
            auto ch = channel_event.first;
            auto ready = channel_event.second;
            ch->handleEvent(ready);
        }
    }
}

void EventLoop::updateChannel(Channel *ch)
{
    ep->updateChannel(ch);
}

void EventLoop::removeChannel(Channel *ch)
{
    ep->remove(ch);
}
