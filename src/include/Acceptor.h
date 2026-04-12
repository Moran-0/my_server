#pragma once
#include <functional>
#include <memory>

class EventLoop;
class Socket;
class InetAddress;
class Channel;

class Acceptor
{
private:
  std::unique_ptr<Socket> sock;
  std::unique_ptr<InetAddress> addr;
  std::unique_ptr<Channel> acceptChannel;

public:
    Acceptor(EventLoop *_loop);
    ~Acceptor() = default;
    void acceptConnection();
    std::function<void(int)> newConnectionCallback;
    void setNewConnectionCallback(std::function<void(int)> cb);
};