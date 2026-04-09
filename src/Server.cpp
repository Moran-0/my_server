#include "Server.h"
#include "Socket.h"
#include "EventLoop.h"
#include "InetAddress.h"
#include "Channel.h"
#include "Acceptor.h"
#include "Buffer.h"
#include "Connection.h"
#include "ThreadPool.h"

#include <iostream>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <functional>

#define READ_BUFFER 1024
using std::cout;
using std::endl;
Server::Server(EventLoop *_loop) : mainReactor(_loop)
{
    acceptor = new Acceptor(mainReactor);
    acceptor->setNewConnectionCallback(std::bind(&Server::newConnection, this, std::placeholders::_1));
    int size = std::thread::hardware_concurrency();
    threadPool = new ThreadPool(size);
    for (int i = 0; i < size; ++i)
    {
        subReactors.push_back(new EventLoop());
    }
    for (int i = 0; i < size; ++i)
    {
        std::function<void()> sub_loop = std::bind(&EventLoop::loop, subReactors.at(i));
        threadPool->addTask(sub_loop); // 每个线程池都执行EventLoop的loop方法，通过epoll监听事件
    }
    cout << "Server " << " Start!" << endl;
}

Server::~Server()
{
    delete acceptor;
    acceptor = nullptr;
    delete threadPool;
    threadPool = nullptr;
}

void Server::deleteConnection(int fd)
{
    std::lock_guard<std::mutex> lock(connections_mtx);
    connections.erase(fd);
}

void Server::newConnection(Socket *sk)
{
    if (sk->getFd() == -1)
    {
        throw std::runtime_error("无效套接字");
    }
    int randReactor = sk->getFd() % subReactors.size(); // 全随机调度策略
    auto connect = std::make_shared<Connection>(subReactors.at(randReactor), sk);
    connect->setDeleteConnectionCallback(std::bind(&Server::deleteConnection, this, std::placeholders::_1));
    connect->registerChannel();
    std::lock_guard<std::mutex> lock(connections_mtx);
    connections[sk->getFd()] = connect;
}
