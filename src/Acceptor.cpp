#include "Acceptor.h"
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"
#include "InetAddress.h"
#include <iostream>

using std::cout;
using std::endl;

Acceptor::Acceptor(EventLoop* _loop, const char* ip, const int port) {
    m_sock = std::make_unique<Socket>();
    m_addr = std::make_unique<InetAddress>(ip, port); // new InetAddress("127.0.0.1", 8888);
    m_sock->bind(*m_addr);
    m_sock->listen();
    // m_sock->setNonBlocking(); // 处理连接的时间较少，采用阻塞式socket
    m_channel = std::make_unique<Channel>(_loop, m_sock->getFd());
    std::function<void()> cb = [this]() { this->AcceptConnection(); };
    m_channel->setReadCallback(cb);
    m_channel->enableRead();
}

void Acceptor::AcceptConnection() {
    InetAddress clnt_addr;
    int clnt_fd = m_sock->accept(clnt_addr);
    cout << "Connect to client " << clnt_fd << "IP:" << inet_ntoa(clnt_addr.getAddr().sin_addr) << " PORT:" << clnt_addr.getAddr().sin_port
         << '\n';
    if (newConnectionCallback) {
        newConnectionCallback(clnt_fd);
    }
}

void Acceptor::SetNewConnectionCallback(std::function<void(int)> cb) {
    newConnectionCallback = std::move(cb);
}
