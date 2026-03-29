#pragma once
#include <unordered_map>
class EventLoop;
class Socket;
class Acceptor;
class Connection;
class Server
{
private:
    EventLoop *loop;
    Acceptor *acceptor;
    std::unordered_map<int, Connection *> connections;

public:
    Server(EventLoop *_loop);
    ~Server();
    void deleteConnection(Socket *_sock);
    void newConnection(Socket *sk);
};