#pragma once
#include <sys/epoll.h>
#include <vector>
class Channel;
class Epoll
{
private:
    int epoll_fd;
    std::vector<epoll_event> events;

public:
    Epoll();
    ~Epoll();
    // void add(int fd, uint32_t events);
    // void modify(int fd, uint32_t events);
    void remove(int fd);
    int wait(std::vector<epoll_event> &events, int timeout);
    // std::vector<epoll_event> poll(int timeout = -1);
    std::vector<Channel *> poll(int timeout = -1);
    void updateChannel(Channel *channel);
};