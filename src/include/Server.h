#pragma once
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <functional>

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
    std::unordered_map<int, std::unique_ptr<TcpConnection>> m_connections;
    std::vector<std::unique_ptr<EventLoop>> m_subReactors;
    std::unique_ptr<ThreadPool> m_threadPool;
    std::mutex m_connections_mtx;

    std::function<void(TcpConnection*)> m_messageCallback;

  public:
    TcpServer();
    ~TcpServer() = default;
    void start();
    void CloseConnection(int fd);
    void CreateConnection(int sock_fd);
    void SetMessageCallback(std::function<void(TcpConnection*)> cb) {
        m_messageCallback = std::move(cb);
    }
};
