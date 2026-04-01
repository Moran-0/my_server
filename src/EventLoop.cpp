#include "EventLoop.h"
#include "Epoll.h"
#include "Channel.h"
#include "ThreadPool.h"

EventLoop::EventLoop() : ep(nullptr), quit(false)
{
    ep = new Epoll();
    thread_pool = new ThreadPool();
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
        for (const auto ch : chs)
        {
            thread_pool->addTask(std::bind(&Channel::handleEvent, ch));
        }
    }
}

void EventLoop::updateChannel(Channel *ch)
{
    ep->updateChannel(ch);
}
