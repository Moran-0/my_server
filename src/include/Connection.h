#pragma once
#include <functional>
#include <memory>

class Socket;
class Channel;
class EventLoop;
class Buffer;
/// @brief std::enable_shared_from_this 是一个辅助类模板，允许从当前对象内部获取指向自身的 std::shared_ptr。（无法安全地从 this 创建 shared_ptr）
class Connection : public std::enable_shared_from_this<Connection>
{
private:
    EventLoop *loop;
    Socket *sock;
    Channel *clnt_ch;
    std::function<void(int)> deleteConnectionCallback;
    Buffer *readBuffer;

public:
    Connection(EventLoop *_loop, Socket *_sock);
    ~Connection();
    void registerChannel();
    bool echo(int sockfd);
    void setDeleteConnectionCallback(std::function<void(int)>);
    bool send(int sockfd);
};
