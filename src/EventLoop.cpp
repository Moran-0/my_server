#include "EventLoop.h"
#include "Epoll.h"
#include "Channel.h"
#include "ThreadPool.h"
#include <iostream>
#include <sys/eventfd.h>
#include <unistd.h>

EventLoop::EventLoop() : m_ep(nullptr), m_quit(false), m_callingTodoList(false) {
    m_ep = std::make_unique<Epoll>();
    m_wakeupFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    m_wakeupChannel = std::make_unique<Channel>(this, m_wakeupFd);
    m_wakeupChannel->setReadCallback([this]() { this->HandleRead(); });
    m_wakeupChannel->enableRead();
}

void EventLoop::loop()
{
    m_threadId = CurrentThread::Tid(); // 获取执行loop函数的线程ID
    while (!m_quit) {
        auto chs = m_ep->poll();
        for (auto &channel_event : chs)
        {
            auto ch = channel_event.first;
            auto ready = channel_event.second;
            ch->HandleEvent(ready);
        }
        DoTodoList();
    }
}

void EventLoop::quit() {
    m_quit = true;
}

void EventLoop::updateChannel(Channel *ch)
{
    m_ep->updateChannel(ch);
}

void EventLoop::removeChannel(Channel *ch)
{
    m_ep->remove(ch);
}

void EventLoop::DoTodoList() {
    m_callingTodoList = true;
    std::vector<std::function<void()>> todoListCopy;
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        todoListCopy.swap(m_todoList);
    }
    for (const auto& func : todoListCopy) {
        func();
    }
    m_callingTodoList = false;
}

void EventLoop::HandleRead() const {
    uint64_t readBytes = 1;
    ssize_t readedBytes = read(m_wakeupFd, &readBytes, sizeof(readBytes));
    (void)readedBytes; // 避免未使用变量的编译警告
    if (readedBytes != sizeof(readBytes)) {
        throw std::runtime_error("EventLoop::HandleRead() - read error");
    }
}

void EventLoop::RunOneFunc(const std::function<void()>& func) {
    if (IsInLoopThread()) {
        func(); // 如果当前线程就是EventLoop所在的线程，直接执行func
    } else {
        QueueFunc(func);
    }
}

void EventLoop::QueueFunc(const std::function<void()>& func) {
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_todoList.push_back(func);
    }
    // 如果当前线程不是EventLoop所在的线程，或者当前正在执行m_todoList中的任务，则需要唤醒EventLoop所在的线程
    // 以确保func能够被及时执行
    if (!IsInLoopThread() || m_callingTodoList) {
        uint64_t one = 1;
        ssize_t writtenBytes = write(m_wakeupFd, &one, sizeof(one));
        (void)writtenBytes; // 避免未使用变量的编译警告
        if (writtenBytes != sizeof(one)) {
            throw std::runtime_error("EventLoop::QueueFunc() - write error");
        }
    }
}
