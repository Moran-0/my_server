#pragma once
#include <functional>
#include <memory>

class Socket;
class Channel;
class EventLoop;
class Buffer;
class Connection
{
private:
    EventLoop *loop;
    std::unique_ptr<Socket> sock;
    std::unique_ptr<Channel> clnt_ch;
    std::function<void(int)> deleteConnectionCallback;
    Buffer *readBuffer;

public:
  Connection(EventLoop* _loop, int sock_fd);
  ~Connection();
  void registerChannel();
  bool echo(int sockfd);
  void setDeleteConnectionCallback(std::function<void(int)>);
  bool send(int sockfd);
};
