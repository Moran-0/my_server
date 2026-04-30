#pragma once
#include <sys/epoll.h>
#include <utility>
#include <vector>
#include <memory>
#include "Timer.h"

class Channel;
class Epoll
{
private:
    int epoll_fd;
    std::vector<epoll_event> events;

  public:
    using ChannelEvent = std::pair<Channel*, uint32_t>;
    Epoll();
    ~Epoll();
    void remove(Channel* channel);
    std::vector<ChannelEvent> poll(int timeout = -1);
    void updateChannel(Channel* channel);

  private:
    int wait(std::vector<epoll_event>& events, int timeout);
};
