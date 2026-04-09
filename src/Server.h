#pragma once
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>
class EventLoop;
class Socket;
class Acceptor;
class Connection;
class ThreadPool;
class Server
{
private:
    // EventLoop *loop;
    EventLoop *mainReactor;
    Acceptor *acceptor;
    std::unordered_map<int, std::shared_ptr<Connection>> connections;
    std::vector<EventLoop *> subReactors;
    ThreadPool *threadPool;
    std::mutex connections_mtx;

public:
    Server(EventLoop *_loop);
    ~Server();
    void deleteConnection(int fd);
    void newConnection(Socket *sk);
};
