#pragma once
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <functional>
#include "EventLoopThreadPool.h"

class EventLoop;
class Socket;
class Acceptor;
class TcpConnection;
class ThreadPool;
class TcpServer {
  private:
    // EventLoop *loop;
    std::unique_ptr<EventLoop> m_mainReactor;
    std::unique_ptr<Acceptor> m_acceptor;
    std::unordered_map<int, std::shared_ptr<TcpConnection>> m_connections;
    std::unique_ptr<EventLoopThreadPool> m_subReactorsPool;
    // std::vector<std::unique_ptr<EventLoop>> m_subReactors;
    // std::unique_ptr<ThreadPool> m_threadPool;
    std::mutex m_connections_mtx;

    std::function<void(const std::shared_ptr<TcpConnection>&)> m_messageCallback;
    std::function<void(const std::shared_ptr<TcpConnection>&)> m_connectCallback;

  public:
    TcpServer();
    ~TcpServer() = default;
    void start();
    void CloseConnection(const std::shared_ptr<TcpConnection>& conn);
    void CloseConnectionInLoop(const std::shared_ptr<TcpConnection>& conn);
    void CreateConnection(int sock_fd);
    void SetMessageCallback(std::function<void(const std::shared_ptr<TcpConnection>&)> cb) {
        m_messageCallback = std::move(cb);
    }
    void SetConnectCallback(std::function<void(const std::shared_ptr<TcpConnection>&)> cb) {
        m_connectCallback = std::move(cb);
    }
};
