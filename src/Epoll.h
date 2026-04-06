#pragma once
#include <sys/epoll.h>
#include <utility>
#include <vector>
class Channel;
class Epoll
{
private:
    int epoll_fd;
    std::vector<epoll_event> events;

public:
    using ChannelEvent = std::pair<Channel *, uint32_t>;
    Epoll();
    ~Epoll();
    // void add(int fd, uint32_t events);
    // void modify(int fd, uint32_t events);
    void remove(Channel *channel);
    int wait(std::vector<epoll_event> &events, int timeout);
    // std::vector<epoll_event> poll(int timeout = -1);
    std::vector<ChannelEvent> poll(int timeout = -1);
    void updateChannel(Channel *channel);
};
