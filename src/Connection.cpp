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

using std::cout;
using std::endl;

constexpr int READ_BUFFER = 1024;

TcpConnection::TcpConnection(EventLoop* _loop, int sock_fd) : m_loop(_loop) {
    m_sock = std::make_unique<Socket>(sock_fd);
    m_sock->setNonBlocking();
    m_channel = std::make_unique<Channel>(m_loop, m_sock->getFd());
    m_channel->useET();
    m_channel->setReadCallback([this]() { this->HandleMessage(); });
    m_readBuffer = std::make_unique<Buffer>();
    m_writeBuffer = std::make_unique<Buffer>();
    m_state = State::Connected;
}
/// @brief 建立连接，监听可读事件
void TcpConnection::EstablishConnection() {
    int sockfd = m_sock->getFd();
    m_channel->enableRead();
}
/// @brief 读取缓冲区数据
void TcpConnection::Read() {
    if (m_state != State::Connected) {
        throw std::runtime_error("当前未建立连接");
    }
    m_readBuffer->Clear();
    ReadNonBlocking();
}

void TcpConnection::Write() {
    if (m_state != State::Connected) {
        throw std::runtime_error("当前未建立连接");
    }
    WriteNonBlocking();
    m_writeBuffer->Clear();
}

bool TcpConnection::echo(int sockfd) {
    char buf[READ_BUFFER];
    while (true)
    {
        // std::fill(buf, buf + READ_BUFFER, 0);
        memset(buf, 0, READ_BUFFER); // 性能更优
        auto byte_read = read(sockfd, buf, READ_BUFFER);
        if (byte_read > 0)
        {
            cout << "Get messages from " << sockfd << ":" << buf << '\n';
            m_readBuffer->append(buf, byte_read);
            // write(sockfd, buf, sizeof(buf));
        }
        else if (byte_read == -1 && errno == EINTR)
        {
            // 客户端正常中断
            cout << "contineu reading..." << '\n';
            continue;
        }
        else if (byte_read == -1 && ((errno == EAGAIN) || (errno == EWOULDBLOCK)))
        {
            // 非阻塞IO表示数据全部读取完毕
            cout << "Finish reading once" << '\n';
            if (!send(sockfd))
            {
                m_channel->disableAll();
                m_loop->removeChannel(m_channel.get());
                return true;
            }
            m_readBuffer->Clear();
            break;
        }
        else if (byte_read == 0)
        {
            cout << "EOF, client " << sockfd << " disconnected!" << '\n';
            m_channel->disableAll();
            m_loop->removeChannel(m_channel.get());
            return true;
        }
    }
    return false;
}

void TcpConnection::setCloseConnectionCallback(std::function<void(int)> cb) {
    m_closeConnectionCallback = std::move(cb);
}
void TcpConnection::SetMessageCallback(std::function<void(TcpConnection*)> cb) {
    m_messageCallback = std::move(cb);
}

/// @brief 处理接收到的消息
void TcpConnection::HandleMessage() {
    Read();
    if (m_messageCallback) {
        m_messageCallback(this);
    }
}
/// @brief 处理关闭请求
void TcpConnection::HandleClose() {
    if (m_state != State::Closed) {
        m_state = State::Closed;
        m_channel->disableAll();
        m_loop->removeChannel(m_channel.get());
        if (m_closeConnectionCallback) {
            m_closeConnectionCallback(m_sock->getFd());
        }
    }
}

bool TcpConnection::send(int sockfd) {
    const char* buf = m_readBuffer->c_str();
    int data_size = static_cast<int>(m_readBuffer->size());
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
    int sockfd = m_sock->getFd();
    char buf[READ_BUFFER];
    while (true) {
        // std::fill(buf, buf + READ_BUFFER, 0);
        memset(buf, 0, READ_BUFFER); // 性能更优
        auto byte_read = read(sockfd, buf, READ_BUFFER);
        if (byte_read > 0) {
            cout << "Get messages from " << sockfd << ":" << buf << '\n';
            m_readBuffer->append(buf, byte_read);
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
    int sockfd = m_sock->getFd();
    auto data_size = m_writeBuffer->size();
    auto data_left = data_size; // 未发送的数据大小
    char buf[data_size];
    std::copy_n(m_writeBuffer->c_str(), data_size, buf);
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
