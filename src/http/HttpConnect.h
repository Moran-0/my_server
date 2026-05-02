#pragma once
#include <functional>
#include <memory>
#include "Buffer.h"
#include "HttpRequest.h"
#include "Channel.h"
#include "Timer.h"
class EventLoop;

class HttpConnect : public NoCopy, public NoMove, public std::enable_shared_from_this<HttpConnect> {
  public:
    enum State { Invalid = 1, HandShaking, Connected, Closed, Failed };

  private:
    EventLoop* m_loop; // 外部传入，HttpConnect不负责管理生命周期
    int m_connectedFd;
    std::unique_ptr<Channel> m_channel;
    std::function<void(const std::shared_ptr<HttpConnect>&)> m_onClose;
    std::function<void(const std::shared_ptr<HttpConnect>&)> m_onConnect;
    std::function<void(const std::shared_ptr<HttpConnect>&)> m_onRequest; // 处理HTTP请求的回调函数
    std::function<void(const std::shared_ptr<HttpConnect>&)> m_onError;   // 处理HTTP请求错误的回调函数

    std::unique_ptr<Buffer> m_readBuffer;
    std::unique_ptr<Buffer> m_writeBuffer;
    State m_state;
    HttpRequest m_request;
    std::weak_ptr<Timer> m_timer; // 连接对应的定时器，定时器过期时关闭连接

  public:
    HttpConnect(EventLoop* _loop, int sock_fd);
    ~HttpConnect();

    void EstablishConnection();
    void DestroyConnection();

    EventLoop* GetLoop() const {
        return m_loop;
    }
    int GetFd() const {
        return m_connectedFd;
    }

    void Write();
    void SetOnCloseCallback(std::function<void(const std::shared_ptr<HttpConnect>&)> cb) {
        m_onClose = std::move(cb);
    }
    void SetOnConnectCallback(std::function<void(const std::shared_ptr<HttpConnect>&)> cb) {
        m_onConnect = std::move(cb);
    }
    void SetOnRequestCallback(std::function<void(const std::shared_ptr<HttpConnect>&)> cb) {
        m_onRequest = std::move(cb);
    }
    void SetOnErrorCallback(std::function<void(const std::shared_ptr<HttpConnect>&)> cb) {
        m_onError = std::move(cb);
    }
    void HandleClose();                                   // 处理关闭请求
    void HandleRequest();                                 // 处理HTTP请求
    void HandleError(int errorNum, std::string errorMsg); // 处理HTTP请求错误

    void LinkTimer(const std::shared_ptr<Timer>& timer) {
        m_timer = timer;
    }
    void SeperateTimer(); // 分离定时器

    State GetState() {
        return m_state;
    };
    const HttpRequest& GetRequest() const {
        return m_request;
    }

    void Send(std::string response) {
        SetSendBuffer(response.c_str());
        Write();
    }

  private:
    void ReadNonBlocking();
    void WriteNonBlocking();
    void SetSendBuffer(const char* str) {
        m_writeBuffer->RetrieveAll();
        m_writeBuffer->Append(str);
    };
};
