#pragma once
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>
class EventLoop;
class Socket;
class Acceptor;
class Connection;
class ThreadPool;
class Server
{
private:
    // EventLoop *loop;
  std::unique_ptr<EventLoop> mainReactor;
  std::unique_ptr<Acceptor> acceptor;
  std::unordered_map<int, std::unique_ptr<Connection>> connections;
  std::vector<std::unique_ptr<EventLoop>> subReactors;
  std::unique_ptr<ThreadPool> threadPool;
  std::mutex connections_mtx;

public:
  Server();
  ~Server() = default;
  void start();
  void deleteConnection(int fd);
  void newConnection(int sock_fd);
};
