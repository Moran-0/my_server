#include "Acceptor.h"
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"
#include "InetAddress.h"
#include <iostream>

using std::cout;
using std::endl;

Acceptor::Acceptor(EventLoop *_loop) : loop(_loop)
{
    sock = new Socket();
    addr = new InetAddress("127.0.0.1", 8888);
    sock->bind(*addr);
    sock->listen();
    sock->setNonBlocking();
    acceptChannel = new Channel(loop, sock->getFd());
    std::function<void()> cb = std::bind(&Acceptor::acceptConnection, this);
    acceptChannel->setCallback(cb);
    acceptChannel->EnableReading();
}

Acceptor::~Acceptor()
{
    delete acceptChannel;
    acceptChannel = nullptr;
    delete addr;
    addr = nullptr;
    delete sock;
    sock = nullptr;
}

void Acceptor::acceptConnection()
{
    InetAddress *clnt_addr = new InetAddress();
    Socket *clnt_sock = new Socket(sock->accept(clnt_addr)); // 内存由server对象负责释放
    cout << "Connect to client " << clnt_sock->getFd() << "IP:" << inet_ntoa(clnt_addr->addr.sin_addr) << " PORT:" << clnt_addr->addr.sin_port << endl;
    clnt_sock->setNonBlocking();
    newConnectionCallback(clnt_sock);
    delete clnt_addr;
}

void Acceptor::setNewConnectionCallback(std::function<void(Socket *)> cb)
{
    newConnectionCallback = cb;
}