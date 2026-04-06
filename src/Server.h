#pragma once
#include <memory>
#include <mutex>
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
    std::unordered_map<int, std::shared_ptr<Connection>> connections;
    std::mutex connections_mtx;

public:
    Server(EventLoop *_loop);
    ~Server();
    void deleteConnection(int fd);
    void newConnection(Socket *sk);
};
