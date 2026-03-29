#include "Epoll.h"
#include "Util.h"
#include <unistd.h>
#include <string>
#include <string.h>
#include "Channel.h"

constexpr int MAX_EVENTS = 1024;

Epoll::Epoll() : epoll_fd(-1), events()
{
    epoll_fd = epoll_create1(0);
    errif(epoll_fd == -1, "Failed to create epoll instance");
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
// void Epoll::add(int fd, uint32_t events)
// {
//     epoll_event ev;
//     ev.events = events;
//     ev.data.fd = fd;
//     errif(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) == -1, "Failed to add file descriptor to epoll");
// }
int Epoll::wait(std::vector<epoll_event> &events, int timeout)
{
    int num_events = epoll_wait(epoll_fd, events.data(), MAX_EVENTS, timeout);
    errif(num_events == -1, "Failed to wait for epoll events");
    return num_events;
}
// std::vector<epoll_event> Epoll::poll(int timeout)
// {
//     std::vector<epoll_event> triggered_events;
//     int num_events = wait(events, timeout);
//     errif(num_events == -1, "Failed to poll epoll events");
//     for (int i = 0; i < num_events; ++i)
//     {
//         triggered_events.push_back(events[i]);
//     }
//     return triggered_events;
// }

std::vector<Channel *> Epoll::poll(int timeout)
{
    std::vector<Channel *> channels;
    int nfds = wait(events, timeout);
    for (int i = 0; i < nfds; ++i)
    {
        auto ch = static_cast<Channel *>(events.at(i).data.ptr);
        ch->setRevents(events.at(i).events);
        channels.push_back(ch);
    }
    return channels;
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
        errif(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) == -1, "epoll_ctl add failed!");
        channel->setInEpoll();
    }
    else
    {
        errif(epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &event) == -1, "epoll_ctl modify failed!");
    }
}
