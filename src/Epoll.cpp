#include "Epoll.h"
#include "Util.h"
#include <unistd.h>
#include <string>
#include <cstring>
#include <iostream>

#include "Channel.h"

constexpr int MAX_EVENTS = 1024;

Epoll::Epoll() : epoll_fd(-1), events()
{
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        std::cout << "Failed to create epoll instance: " << strerror(errno) << '\n';
    }
    events.resize(MAX_EVENTS);
    std::fill(events.begin(), events.end(), epoll_event{0});
}
Epoll::~Epoll()
{
    if (epoll_fd != -1)
    {
        close(epoll_fd);
        epoll_fd = -1;
    }
}

int Epoll::wait(std::vector<epoll_event> &events, int timeout)
{
    int num_events = epoll_wait(epoll_fd, events.data(), MAX_EVENTS, timeout);
    if (num_events == -1) {
        if (errno != EINTR) {
            std::cout << "Failed to wait for epoll events: " << strerror(errno) << '\n';
        }
        return 0; // 中断信号导致的错误，返回0表示没有事件发生
    }
    return num_events;
}

std::vector<Epoll::ChannelEvent> Epoll::poll(int timeout)
{
    std::vector<ChannelEvent> channels;
    int nfds = wait(events, timeout);
    for (int i = 0; i < nfds; ++i)
    {
        auto* ch(static_cast<Channel*>(events.at(i).data.ptr));
        uint32_t ready = events.at(i).events;
        channels.emplace_back(ch, ready);
    }
    return channels;
}

void Epoll::remove(Channel *channel)
{
    if (channel == nullptr || !channel->getInEpoll())
    {
        return;
    }
    int fd = channel->getFd();
    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr) == -1) {
        std::cout << "Failed to remove fd " << fd << " from epoll instance" << '\n';
    }
    channel->setInEpoll(false);
}

void Epoll::updateChannel(Channel *channel)
{
    int fd = channel->getFd();
    epoll_event event;
    bzero(&event, sizeof(event));
    event.data.ptr = channel;
    event.events = channel->getEvents();
    if (!channel->getInEpoll())
    {
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) == -1) {
            std::cout << "Failed to add fd " << fd << " to epoll instance" << '\n';
        }
        channel->setInEpoll();
    }
    else
    {
        if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &event) == -1) {
            std::cout << "Failed to modify fd " << fd << " in epoll instance" << '\n';
        }
    }
}
