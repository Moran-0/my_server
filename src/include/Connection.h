#pragma once
#include <functional>
#include <memory>
#include "Macro.h"
#include "Buffer.h"
#include "Common.h"

class Socket;
class Channel;
class EventLoop;
class Buffer;
class TcpConnection : public NoCopy, public NoMove {
  public:
    enum State { Invalid = 1, HandShaking, Connected, Closed, Failed };

  private:
    EventLoop* m_loop; // 外部传入，TcpConnection不负责管理生命周期
    std::unique_ptr<Socket> m_sock;
    std::unique_ptr<Channel> m_channel;
    std::function<void(int)> m_closeConnectionCallback;
    std::function<void(TcpConnection*)> m_messageCallback;
    std::unique_ptr<Buffer> m_readBuffer;
    std::unique_ptr<Buffer> m_writeBuffer;
    State m_state;

  public:
    TcpConnection(EventLoop* _loop, int sock_fd);
    ~TcpConnection() = default;

    void EstablishConnection();

    void Read();
    void Write();
    bool echo(int sockfd); // 暂时弃用，后续重构时删除
    void setCloseConnectionCallback(std::function<void(int)>);
    void SetMessageCallback(std::function<void(TcpConnection*)>);
    void HandleMessage(); // 接受到message时进行处理
    void HandleClose();   // 处理关闭请求

    State GetState();

    void SetSendBuffer(const char* str);
    void Send(const char* str) {
        SetSendBuffer(str);
        Write();
    }
    void Send(const std::string& str) {
        SetSendBuffer(str.c_str());
        Write();
    }
    bool send(int sockfd); // 发送数据，返回是否成功。暂时弃用

  private:
    void ReadNonBlocking();
    void WriteNonBlocking();
};

/// @brief 返回连接状态
/// @return
inline TcpConnection::State TcpConnection::GetState() {
    return m_state;
}

inline void TcpConnection::SetSendBuffer(const char* str) {
    m_writeBuffer->SetBuf(str);
}
