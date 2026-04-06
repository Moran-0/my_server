#include "Server.h"
#include "Socket.h"
#include "EventLoop.h"
#include "InetAddress.h"
#include "Channel.h"
#include "Acceptor.h"
#include "Buffer.h"
#include "Connection.h"

#include <iostream>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <functional>

#define READ_BUFFER 1024
using std::cout;
using std::endl;
Server::Server(EventLoop *_loop) : loop(_loop)
{
    acceptor = new Acceptor(loop);
    acceptor->setNewConnectionCallback(std::bind(&Server::newConnection, this, std::placeholders::_1));
    cout << "Server " << " Start!" << endl;
}

Server::~Server()
{
    delete acceptor;
    acceptor = nullptr;
}

void Server::deleteConnection(int fd)
{
    std::lock_guard<std::mutex> lock(connections_mtx);
    connections.erase(fd);
}

void Server::newConnection(Socket *sk)
{
    auto connect = std::make_shared<Connection>(loop, sk);
    connect->setDeleteConnectionCallback(std::bind(&Server::deleteConnection, this, std::placeholders::_1));
    connect->registerChannel();
    std::lock_guard<std::mutex> lock(connections_mtx);
    connections[sk->getFd()] = connect;
}
