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
#include <cerrno>
#include <unistd.h>
#include <functional>

#define READ_BUFFER 1024
using std::cout;
using std::endl;
Server::Server()
{
    mainReactor = std::make_unique<EventLoop>();
    acceptor = std::make_unique<Acceptor>(mainReactor.get());
    // acceptor->setNewConnectionCallback(std::bind(&Server::newConnection, this,
    // std::placeholders::_1));
    acceptor->setNewConnectionCallback([this](int sock_fd) { this->newConnection(sock_fd); });
    int size = std::thread::hardware_concurrency();
    threadPool = std::make_unique<ThreadPool>(size);
    for (int i = 0; i < size; ++i)
    {
        subReactors.push_back(std::make_unique<EventLoop>());
    }
}
void Server::start()
{
    for (const auto& reactor : subReactors)
    {
        std::function<void()> sub_loop = [loop = reactor.get()]() { loop->loop(); };
        threadPool->addTask(sub_loop); // 每个线程池都执行EventLoop的loop方法，通过epoll监听事件
    }
    cout << "Server " << " Start!" << '\n';
    mainReactor->loop();
}

void Server::deleteConnection(int fd)
{
    std::lock_guard<std::mutex> lock(connections_mtx);
    connections.erase(fd);
}

void Server::newConnection(int sock_fd)
{
    if (sock_fd == -1)
    {
        throw std::runtime_error("无效套接字");
    }
    int randReactor = sock_fd % subReactors.size(); // 全随机调度策略
    auto connect = std::make_unique<Connection>(subReactors.at(randReactor).get(), sock_fd);
    connect->setDeleteConnectionCallback([this](int fd) { this->deleteConnection(fd); });
    // connect->setDeleteConnectionCallback([server = this](int fd){server->deleteConnection(fd);});
    connect->registerChannel();
    std::lock_guard<std::mutex> lock(connections_mtx);
    connections[sock_fd] = std::move(connect);
}
