#include "HttpServer.h"
#include "Socket.h"
#include "EventLoop.h"
#include "InetAddress.h"
#include "Channel.h"
#include "Acceptor.h"
#include "Buffer.h"
#include "ThreadPool.h"
#include "HttpResponse.h"
#include "HttpConfig.h"

#include <iostream>
#include <fcntl.h>
#include <cerrno>
#include <unistd.h>
#include <functional>

using std::cout;
using std::endl;
HttpServer::HttpServer() {
    m_mainReactor = std::make_unique<EventLoop>();
    m_acceptor = std::make_unique<Acceptor>(m_mainReactor.get(), "127.0.0.1", 8888);
    m_acceptor->SetNewConnectionCallback([this](int sock_fd) { this->CreateConnection(sock_fd); });
    int size = std::thread::hardware_concurrency();
    m_subReactorsPool = std::make_unique<EventLoopThreadPool>(m_mainReactor.get(), size);
    m_requestCallback = [this](const std::shared_ptr<HttpConnect>& conn) { this->OnRequest(conn); };
}

void HttpServer::start() {
    m_subReactorsPool->Start(); // 创建并启动线程池，创建subReactor线程，并让每个subReactor线程的EventLoop开始事件循环
    cout << "HttpServer " << " Start!" << '\n';
    m_mainReactor->loop();
}

void HttpServer::CloseConnection(const std::shared_ptr<HttpConnect>& conn) {
    cout << "[CloseConnection] Close connection in thread " << CurrentThread::Tid() << '\n';
    m_mainReactor->RunOneFunc([this, conn]() { this->CloseConnectionInLoop(conn); });
}

/// @brief 在loop当前线程中关闭连接，移除连接对象，并将该连接所属channel从EventLoop中移除。由于loop所属线程唯一，因此不需要加锁保护m_connections
/// @param conn 连接对象
void HttpServer::CloseConnectionInLoop(const std::shared_ptr<HttpConnect>& conn) {
    cout << "[CloseConnectionInLoop] Close connection in thread " << CurrentThread::Tid() << '\n';
    m_connections.erase(conn->GetFd());
    // 将该连接所属channel从EventLoop中移除
    auto* conn_loop = conn->GetLoop();
    conn_loop->QueueFunc([conn_loop, conn]() {
        conn->DestroyConnection();
    }); // 当前在mainReactor线程中，一定不在conn所属的EventLoop线程中，因此需要通过QueueFunc将DestroyConnection函数添加到conn所属EventLoop的任务队列中，由conn所属EventLoop所在的线程来执行DestroyConnection函数，从而安全地移除连接对象和channel对象
}

void HttpServer::CreateConnection(int sock_fd) {
    if (sock_fd == -1) {
        throw std::runtime_error("无效套接字");
    }
    auto* work_loop = m_subReactorsPool->GetNextLoop();
    auto connect = std::make_shared<HttpConnect>(work_loop, sock_fd);
    connect->SetOnCloseCallback([this](const std::shared_ptr<HttpConnect>& conn) { this->CloseConnection(conn); });
    connect->SetOnRequestCallback(m_requestCallback);
    connect->SetOnConnectCallback(m_connectCallback);
    {
        std::lock_guard<std::mutex> lock(m_connections_mtx);
        m_connections[sock_fd] = connect;
    }
    work_loop->RunOneFunc([work_loop, connect]() {
        connect->EstablishConnection();
        work_loop->AddScheduledTask(
            HTTP_DEFAULT_TIMEOUT, [connect]() { connect->HandleClose(); },
            [connect](const std::shared_ptr<Timer>& timer) { connect->LinkTimer(timer); });
    });
}

void HttpServer::OnRequest(const std::shared_ptr<HttpConnect>& conn) {
    auto request = conn->GetRequest();
    HttpResponse response(!request.IsKeepAlive());
    response.SetStatusCode(HttpResponse::K200K);
    response.SetStatusMessage("OK");
    response.SetContentType("text/html");
    std::string body_buff;
    body_buff += "<html><title>Moran's webpage!</title>";
    body_buff += "<body bgcolor=\"ffffff\">";
    body_buff += "<hr><em> Moran's Web Server</em>\n</body></html>";

    response.AddHeader("Content-Length", std::to_string(body_buff.size()));
    response.AddHeader("Server", "Moran's Web Server");
    response.SetBody(body_buff);

    conn->Send(response.message());
    if (!request.IsKeepAlive()) {
        // conn->HandleClose();
        // 效果同上，因为OnRequest本身就是在连接所在线程调用，下述代码只是让线程边界更稳，两者皆可
        auto* connLoop = conn->GetLoop();
        connLoop->RunOneFunc([conn]() { conn->HandleClose(); });
    }
}
