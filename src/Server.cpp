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

void TcpServer::CloseConnection(const std::shared_ptr<TcpConnection>& conn) {
    cout << "[CloseConnection] Close connection in thread " << CurrentThread::Tid() << '\n';
    m_mainReactor->RunOneFunc([this, &conn]() { this->CloseConnectionInLoop(conn); });
}

/// @brief 在loop当前线程中关闭连接，移除连接对象，并将该连接所属channel从EventLoop中移除。由于loop所属线程唯一，因此不需要加锁保护m_connections
/// @param conn 连接对象
void TcpServer::CloseConnectionInLoop(const std::shared_ptr<TcpConnection>& conn) {
    cout << "[CloseConnectionInLoop] Close connection in thread " << CurrentThread::Tid() << '\n';
    m_connections.erase(conn->GetFd());
    // 将该连接所属channel从EventLoop中移除
    auto* conn_loop = conn->GetLoop();
    conn_loop->QueueFunc([conn_loop, &conn]() {
        conn->DestroyConnection();
    }); // 当前在mainReactor线程中，一定不在conn所属的EventLoop线程中，因此需要通过QueueFunc将DestroyConnection函数添加到conn所属EventLoop的任务队列中，由conn所属EventLoop所在的线程来执行DestroyConnection函数，从而安全地移除连接对象和channel对象
}

void TcpServer::CreateConnection(int sock_fd) {
    if (sock_fd == -1)
    {
        throw std::runtime_error("无效套接字");
    }
    int randReactor = sock_fd % m_subReactors.size(); // 全随机调度策略
    auto connect = std::make_shared<TcpConnection>(m_subReactors.at(randReactor).get(), sock_fd);
    connect->SetOnCloseCallback([this](const std::shared_ptr<TcpConnection>& conn) { this->CloseConnection(conn); });
    connect->SetOnMessageCallback(m_messageCallback);
    connect->SetOnConnectCallback(m_connectCallback);
    connect->EstablishConnection();
    std::lock_guard<std::mutex> lock(m_connections_mtx);
    m_connections[sock_fd] = std::move(connect);
}
