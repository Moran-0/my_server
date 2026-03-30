#pragma once
#include <functional>

class Socket;
class Channel;
class EventLoop;
class Buffer;
class Connection
{
private:
    EventLoop *loop;
    Socket *sock;
    Channel *clnt_ch;
    std::function<void(Socket *)> deleteConnectionCallback;
    Buffer *buffer;

public:
    Connection(EventLoop *_loop, Socket *_sock);
    ~Connection();
    void echo(int sockfd);
    void setDelteConnectionCallback(std::function<void(Socket *)>);
};
