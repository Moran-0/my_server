#pragma once
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <functional>
#include "EventLoopThreadPool.h"
#include "HttpConnect.h"

class EventLoop;
class Socket;
class Acceptor;
class ThreadPool;
class HttpServer {
  private:
    // EventLoop *loop;
    std::unique_ptr<EventLoop> m_mainReactor;
    std::unique_ptr<Acceptor> m_acceptor;
    std::unordered_map<int, std::shared_ptr<HttpConnect>> m_connections;
    std::unique_ptr<EventLoopThreadPool> m_subReactorsPool;
    // std::vector<std::unique_ptr<EventLoop>> m_subReactors;
    // std::unique_ptr<ThreadPool> m_threadPool;
    std::mutex m_connections_mtx;

    std::function<void(const std::shared_ptr<HttpConnect>&)> m_requestCallback;
    std::function<void(const std::shared_ptr<HttpConnect>&)> m_connectCallback;

  public:
    HttpServer(const char* ip, int port);
    ~HttpServer() = default;
    void start();
    void CloseConnection(const std::shared_ptr<HttpConnect>& conn);
    void CloseConnectionInLoop(const std::shared_ptr<HttpConnect>& conn);
    void CreateConnection(int sock_fd);
    void SetRequestCallback(std::function<void(const std::shared_ptr<HttpConnect>&)> cb) {
        m_requestCallback = std::move(cb);
    }
    void SetConnectCallback(std::function<void(const std::shared_ptr<HttpConnect>&)> cb) {
        m_connectCallback = std::move(cb);
    }

  private:
    void OnRequest(const std::shared_ptr<HttpConnect>& conn);
};
