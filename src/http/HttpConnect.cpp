#include "HttpConnect.h"
#include "EventLoop.h"
#include "HttpResponse.h"

#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <iostream>
#include "HttpConnect.h"

using std::cout;
using std::endl;

constexpr int READ_BUFFER = 1024;
constexpr int WRITE_BUFFER = 4096;

HttpConnect::HttpConnect(EventLoop* _loop, int sock_fd) : m_loop(_loop), m_connectedFd(sock_fd) {
    fcntl(m_connectedFd, F_SETFL, fcntl(m_connectedFd, F_GETFL) | O_NONBLOCK); // 设置非阻塞IO
    m_channel = std::make_unique<Channel>(m_loop, m_connectedFd);
    m_channel->useET();
    m_channel->setReadCallback([this]() { this->HandleRequest(); });
    m_readBuffer = std::make_unique<Buffer>();
    m_writeBuffer = std::make_unique<Buffer>();
    m_state = State::Connected;
}

HttpConnect::~HttpConnect() {
    ::close(m_connectedFd); // 关闭连接
}

void HttpConnect::EstablishConnection() {
    m_channel->enableRead();
    m_channel->Tie(shared_from_this());
    if (m_onConnect) {
        m_onConnect(shared_from_this()); // 连接建立成功，执行回调函数
    }
}

void HttpConnect::DestroyConnection() {
    m_channel->disableAll();
    m_loop->removeChannel(m_channel.get());
    m_state = State::Closed;
}

void HttpConnect::Write() {
    if (m_state != State::Connected) {
        throw std::runtime_error("当前未建立连接");
    }
    WriteNonBlocking();
    m_writeBuffer->Clear();
}

void HttpConnect::HandleClose() {
    if (m_state != State::Closed) {
        if (m_onClose) {
            m_onClose(shared_from_this());
        }
    }
}

void HttpConnect::HandleRequest() {
    ReadNonBlocking();
    while (true) {
        auto parsed = m_request.Parse(m_readBuffer->str());
        if (parsed == -1) {
            cout << "Failed to parse http request\n";
            m_state = State::Failed;
            m_request.Reset();
            m_readBuffer->Clear();
            HandleError(400, "Bad Request");
            break;
        }
        if (parsed == 0) {
            cout << "Http request is incomplete, continue reading...\n";
            break;
        }
        if (m_request.IsFinish()) {
            cout << "Successfully parsed http request\n";
            if (m_onRequest) {
                m_onRequest(shared_from_this());
            }
            m_request.Reset();
        } else {
            cout << "Http request is incomplete, continue reading...\n";
        }
        m_readBuffer->Erase(0, parsed); // 支持半包和粘包请求，解析完一部分就从缓冲区删除掉，继续读取剩余部分
    }
}

void HttpConnect::HandleError(int errorNum, std::string errorMsg) {
    if (m_onError) {
        m_onError(shared_from_this());
        return;
    }
    errorMsg = " " + errorMsg;
    std::string body_buff;
    std::string header_buff;
    body_buff += "<html><title>哎~出错了</title>";
    body_buff += "<body bgcolor=\"ffffff\">";
    body_buff += std::to_string(errorNum) + errorMsg;
    body_buff += "<hr><em> LinYa's Web Server</em>\n</body></html>";

    header_buff += "HTTP/1.1 " + std::to_string(errorNum) + errorMsg + "\r\n";
    header_buff += "Content-Type: text/html\r\n";
    header_buff += "Connection: Close\r\n";
    header_buff += "Content-Length: " + std::to_string(body_buff.size()) + "\r\n";
    header_buff += "Server: LinYa's Web Server\r\n";
    header_buff += "\r\n";
    std::string response = header_buff + body_buff;
    SetSendBuffer(response.c_str());
    WriteNonBlocking();
}

void HttpConnect::ReadNonBlocking() {
    int sockfd = m_connectedFd;
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

void HttpConnect::WriteNonBlocking() {
    int sockfd = m_connectedFd;
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
