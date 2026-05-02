#include "Connection.h"
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"
#include "Buffer.h"

#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>
#include <algorithm>
#include <fcntl.h>

using std::cout;
using std::endl;

constexpr int READ_BUFFER = 1024;

TcpConnection::TcpConnection(EventLoop* _loop, int sock_fd) : m_loop(_loop), m_connectedFd(sock_fd) {
    fcntl(m_connectedFd, F_SETFL, fcntl(m_connectedFd, F_GETFL) | O_NONBLOCK); // 设置非阻塞IO
    m_channel = std::make_unique<Channel>(m_loop, m_connectedFd);
    m_channel->useET();
    m_channel->setReadCallback([this]() { this->HandleMessage(); });
    m_readBuffer = std::make_unique<Buffer>();
    m_writeBuffer = std::make_unique<Buffer>();
    m_state = State::Connected;
}
TcpConnection::~TcpConnection() {
    ::close(m_connectedFd); // 关闭连接
}
/// @brief 建立连接，监听可读事件
void TcpConnection::EstablishConnection() {
    m_channel->enableRead();
    m_channel->Tie(shared_from_this());
    if (m_onConnect) {
        m_onConnect(shared_from_this()); // 连接建立成功，执行回调函数
    }
}
void TcpConnection::DestroyConnection() {
    m_channel->disableAll();
    m_loop->removeChannel(m_channel.get());
    m_state = State::Closed;
}
/// @brief 读取缓冲区数据
void TcpConnection::Read() {
    if (m_state != State::Connected) {
        throw std::runtime_error("当前未建立连接");
    }
    m_readBuffer->RetrieveAll();
    ReadNonBlocking();
}

void TcpConnection::Write() {
    if (m_state != State::Connected) {
        throw std::runtime_error("当前未建立连接");
    }
    WriteNonBlocking();
    m_writeBuffer->RetrieveAll();
}

/// @brief 处理接收到的消息
void TcpConnection::HandleMessage() {
    Read();
    if (m_onMessage) {
        m_onMessage(shared_from_this());
    }
}
/// @brief 处理关闭请求
void TcpConnection::HandleClose() {
    if (m_state != State::Closed) {
        if (m_onClose) {
            m_onClose(shared_from_this());
        }
    }
}

bool TcpConnection::send(int sockfd) {
    const char* buf = m_readBuffer->Peek();
    int data_size = static_cast<int>(m_readBuffer->ReadableBytes());
    int data_left = data_size;
    while (data_left > 0)
    {
        ssize_t bytes_write = ::send(sockfd, buf + data_size - data_left, data_left, MSG_NOSIGNAL);
        if (bytes_write > 0)
        {
            data_left -= static_cast<int>(bytes_write);
            continue;
        }
        if (bytes_write == -1 && errno == EINTR)
        {
            continue;
        }
        if (bytes_write == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            break;
        }
        return false;
    }
    return true;
}
/// @brief 非阻塞I/O读取
void TcpConnection::ReadNonBlocking() {
    int sockfd = m_connectedFd;
    char buf[READ_BUFFER];
    while (true) {
        // std::fill(buf, buf + READ_BUFFER, 0);
        memset(buf, 0, READ_BUFFER); // 性能更优
        auto byte_read = read(sockfd, buf, READ_BUFFER);
        if (byte_read > 0) {
            cout << "Get messages from " << sockfd << ":" << buf << '\n';
            m_readBuffer->Append(buf, byte_read);
        } else if (byte_read == -1 && errno == EINTR) {
            // 客户端正常中断
            cout << "contineu reading..." << '\n';
            continue;
        } else if (byte_read == -1 && ((errno == EAGAIN) || (errno == EWOULDBLOCK))) {
            // 非阻塞IO表示数据全部读取完毕
            cout << "Finish reading once" << '\n';
            break;
        } else if (byte_read == 0) { // EOF,客户端断开连接
            cout << "EOF, client " << sockfd << " disconnected!" << '\n';
            HandleClose();
            break;
        } else {
            cout << "Other error on client fd " << sockfd << '\n';
            m_state = State::Closed;
            break;
        }
    }
}

/// @brief 非阻塞IO写入
void TcpConnection::WriteNonBlocking() {
    int sockfd = m_connectedFd;
    auto data_size = m_writeBuffer->ReadableBytes();
    auto data_left = data_size; // 未发送的数据大小
    char buf[data_size];
    std::copy_n(m_writeBuffer->Peek(), data_size, buf);
    while (data_left > 0) {
        auto byte_writen = write(sockfd, buf + data_size - data_left, data_left);
        if (byte_writen == -1 && errno == EINTR) {
            cout << "continue writing...\n";
            continue;
        }
        if (byte_writen == -1 && errno == EAGAIN) { // 资源暂时不可用，请稍后重试
            break;
        }
        if (byte_writen == -1) {
            cout << "Other error on client fd " << sockfd << '\n';
            HandleClose();
            break;
        }
        data_left -= byte_writen;
    }
}
