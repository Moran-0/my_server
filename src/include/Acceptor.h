#pragma once
#include <functional>
#include <memory>
#include "Common.h"
class EventLoop;
class Socket;
class InetAddress;
class Channel;

class Acceptor : public NoCopy, NoMove {
  private:
    std::unique_ptr<Socket> m_sock;
    std::unique_ptr<InetAddress> m_addr;
    std::unique_ptr<Channel> m_channel;

  public:
    Acceptor(EventLoop* _loop, const char* ip, const int port);
    ~Acceptor() = default;
    void AcceptConnection();
    std::function<void(int)> newConnectionCallback;
    void SetNewConnectionCallback(std::function<void(int)> cb);
};