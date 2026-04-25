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
TcpServer::TcpServer() {
    m_mainReactor = std::make_unique<EventLoop>();
    m_acceptor = std::make_unique<Acceptor>(m_mainReactor.get(), "127.0.0.1", 8888);
    // m_acceptor->SetNewConnectionCallback(std::bind(&TcpServer::CreateConnection, this,
    // std::placeholders::_1));
    m_acceptor->SetNewConnectionCallback([this](int sock_fd) { this->CreateConnection(sock_fd); });
    int size = std::thread::hardware_concurrency();
    m_threadPool = std::make_unique<ThreadPool>(size);
    for (int i = 0; i < size; ++i)
    {
        m_subReactors.push_back(std::make_unique<EventLoop>());
    }
}
void TcpServer::start() {
    for (const auto& reactor : m_subReactors) {
        std::function<void()> sub_loop = [loop = reactor.get()]() { loop->loop(); };
        m_threadPool->addTask(sub_loop); // 每个线程池都执行EventLoop的loop方法，通过epoll监听事件
    }
    cout << "TcpServer " << " Start!" << '\n';
    m_mainReactor->loop();
}

void TcpServer::CloseConnection(int fd) {
    {
        std::lock_guard<std::mutex> lock(m_connections_mtx);
        m_connections.erase(fd);
    }
    cout << "Close connection with fd " << fd << '\n';
}

void TcpServer::CreateConnection(int sock_fd) {
    if (sock_fd == -1)
    {
        throw std::runtime_error("无效套接字");
    }
    int randReactor = sock_fd % m_subReactors.size(); // 全随机调度策略
    auto connect = std::make_unique<TcpConnection>(m_subReactors.at(randReactor).get(), sock_fd);
    connect->setCloseConnectionCallback([this](int fd) { this->CloseConnection(fd); });
    connect->SetMessageCallback(m_messageCallback);
    // connect->setCloseConnectionCallback([server = this](int fd){server->CloseConnection(fd);});
    connect->EstablishConnection();
    std::lock_guard<std::mutex> lock(m_connections_mtx);
    m_connections[sock_fd] = std::move(connect);
}
