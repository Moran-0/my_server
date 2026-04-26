#pragma once
#include <functional>
#include <memory>
#include "Macro.h"
#include "Buffer.h"
#include "Common.h"

class Channel;
class EventLoop;
class Buffer;
class TcpConnection : public NoCopy, public NoMove, public std::enable_shared_from_this<TcpConnection> {
  public:
    enum State { Invalid = 1, HandShaking, Connected, Closed, Failed };

  private:
    EventLoop* m_loop; // 外部传入，TcpConnection不负责管理生命周期
    int m_connectedFd;
    std::unique_ptr<Channel> m_channel;
    std::function<void(const std::shared_ptr<TcpConnection>&)> m_onClose;
    std::function<void(const std::shared_ptr<TcpConnection>&)> m_onMessage;
    std::function<void(const std::shared_ptr<TcpConnection>&)> m_onConnect;

    std::unique_ptr<Buffer> m_readBuffer;
    std::unique_ptr<Buffer> m_writeBuffer;
    State m_state;

  public:
    TcpConnection(EventLoop* _loop, int sock_fd);
    ~TcpConnection();

    void EstablishConnection();
    void DestroyConnection();

    EventLoop* GetLoop() const {
        return m_loop;
    }
    int GetFd() const {
        return m_connectedFd;
    }

    void Read();
    void Write();
    void SetOnCloseCallback(std::function<void(const std::shared_ptr<TcpConnection>&)> cb) {
        m_onClose = std::move(cb);
    }
    void SetOnMessageCallback(std::function<void(const std::shared_ptr<TcpConnection>&)> cb) {
        m_onMessage = std::move(cb);
    }
    void SetOnConnectCallback(std::function<void(const std::shared_ptr<TcpConnection>&)> cb) {
        m_onConnect = std::move(cb);
    }
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
