#include "EventLoop.h"
#include "Epoll.h"
#include "Channel.h"
#include "ThreadPool.h"
#include <iostream>

EventLoop::EventLoop() : ep(nullptr), quit(false)
{
    ep = new Epoll();
    thread_pool = new ThreadPool();
}

EventLoop::~EventLoop()
{
    delete thread_pool;
    thread_pool = nullptr;
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
            if (ch->getUseThreadPool())
            {
                if (ch->tryStartHandling())
                {
                    thread_pool->addTask([ch, ready]()
                                         {
                                             ch->handleEvent(ready);
                                             ch->finishHandling();
                                         });
                }
            }
            else
            {
                ch->handleEvent(ready); // 连接事件不使用线程池
            }
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
