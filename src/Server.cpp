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

void Server::deleteConnection(Socket *_sock)
{
    auto connect = connections[_sock->getFd()];
    connections.erase(_sock->getFd());
    delete connect;
    connect = nullptr;
}

void Server::newConnection(Socket *sk)
{
    Connection *connect = new Connection(loop, sk); // 内存由deleteConnection函数进行释放（通过Connection对象进行回调）
    connect->setDelteConnectionCallback(std::bind(&Server::deleteConnection, this, std::placeholders::_1));
    connections[sk->getFd()] = connect;
}
