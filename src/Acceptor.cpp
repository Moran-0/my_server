#include "Acceptor.h"
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"
#include "InetAddress.h"
#include <iostream>

using std::cout;
using std::endl;

Acceptor::Acceptor(EventLoop* _loop)
{
    sock = std::make_unique<Socket>();
    addr = std::make_unique<InetAddress>("127.0.0.1", 8888); // new InetAddress("127.0.0.1", 8888);
    sock->bind(*addr);
    sock->listen();
    // sock->setNonBlocking(); // 处理连接的时间较少，采用阻塞式socket
    acceptChannel = std::make_unique<Channel>(_loop, sock->getFd());
    // std::function<void()> cb = std::bind(&Acceptor::acceptConnection, this);
    std::function<void()> cb = [this]() { this->acceptConnection(); };
    acceptChannel->setReadCallback(cb);
    acceptChannel->enableRead();
}

void Acceptor::acceptConnection()
{
    InetAddress clnt_addr;
    int clnt_fd = sock->accept(clnt_addr);
    cout << "Connect to client " << clnt_fd << "IP:" << inet_ntoa(clnt_addr.getAddr().sin_addr) << " PORT:" << clnt_addr.getAddr().sin_port
         << '\n';
    newConnectionCallback(clnt_fd);
}

void Acceptor::setNewConnectionCallback(std::function<void(int)> cb)
{
    newConnectionCallback = std::move(cb);
}
