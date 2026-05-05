#include "HttpConnect.h"
#include "EventLoop.h"
#include "HttpResponse.h"
#include "HttpConfig.h"
#include "Logging.h"

#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <iostream>

using std::cout;
using std::endl;

constexpr int READ_BUFFER = 1024;
// constexpr int WRITE_BUFFER = 4096;

namespace {
std::string ErrorPage(int errorNum, const std::string& errorMsg) {
    const auto code = std::to_string(errorNum);
    return R"(<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>)" + code + " " + errorMsg + R"(</title>
  <style>
    :root {
      color-scheme: light;
      --bg: #f6f7f9;
      --panel: #ffffff;
      --text: #17202e;
      --muted: #667085;
      --line: #d9dee8;
      --accent: #2f6fed;
    }
    * {
      box-sizing: border-box;
    }
    body {
      margin: 0;
      min-height: 100vh;
      display: grid;
      place-items: center;
      padding: 32px;
      font-family: Inter, ui-sans-serif, system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
      background: var(--bg);
      color: var(--text);
    }
    main {
      width: min(560px, 100%);
      padding: 36px;
      border: 1px solid var(--line);
      border-radius: 8px;
      background: var(--panel);
      box-shadow: 0 18px 44px rgba(23, 32, 46, 0.08);
    }
    .code {
      margin: 0 0 14px;
      color: var(--accent);
      font-size: 14px;
      font-weight: 700;
      letter-spacing: 0;
    }
    h1 {
      margin: 0;
      font-size: clamp(28px, 6vw, 44px);
      line-height: 1.08;
      letter-spacing: 0;
    }
    p {
      margin: 16px 0 0;
      color: var(--muted);
      line-height: 1.7;
    }
    a {
      display: inline-block;
      margin-top: 26px;
      color: var(--accent);
      font-weight: 700;
      text-decoration: none;
    }
    a:hover {
      text-decoration: underline;
    }
  </style>
</head>
<body>
  <main>
    <p class="code">HTTP )" + code + R"(</p>
    <h1>)" + errorMsg + R"(</h1>
    <p>The server understood the request, but could not complete it in its current form.</p>
    <a href="/">Return home</a>
  </main>
</body>
</html>)";
}
} // namespace

HttpConnect::HttpConnect(EventLoop* _loop, int sock_fd) : m_loop(_loop), m_connectedFd(sock_fd), m_closeAfterWrite(false) {
    fcntl(m_connectedFd, F_SETFL, fcntl(m_connectedFd, F_GETFL) | O_NONBLOCK); // 设置非阻塞IO
    m_channel = std::make_unique<Channel>(m_loop, m_connectedFd);
    m_channel->UseET();
    m_channel->SetReadCallback([this]() { this->HandleRequest(); });
    m_channel->SetWriteCallback([this]() { this->Write(); });
    m_readBuffer = std::make_unique<Buffer>();
    m_writeBuffer = std::make_unique<Buffer>();
    m_state = State::Connected;
}

HttpConnect::~HttpConnect() {
    ::close(m_connectedFd); // 关闭连接
}

void HttpConnect::EstablishConnection() {
    m_channel->EnableRead();
    m_channel->Tie(shared_from_this());
    if (m_onConnect) {
        m_onConnect(shared_from_this()); // 连接建立成功，执行回调函数
    }
}

void HttpConnect::DestroyConnection() {
    m_channel->DisableAll();
    m_loop->removeChannel(m_channel.get());
    m_state = State::Closed;
}

void HttpConnect::Write() {
    if (m_state == State::Closed) {
        return;
    }
    WriteNonBlocking();
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
    if (auto timer = m_timer.lock()) {
        timer->Update(HTTP_DEFAULT_TIMEOUT);
    }
    while (true) {
        auto parsed = m_request.Parse(m_readBuffer->PeekString());
        if (parsed == -1) {
            LOG_WARN << "Failed to parse http requeset\n";
            m_state = State::Failed;
            m_request.Reset();
            m_readBuffer->RetrieveAll();
            HandleError(400, "Bad Request");
            break;
        }
        if (parsed == 0) {
            LOG_INFO << "Http request is incomplete, wait left data...\n";
            break;
        }
        if (m_request.IsFinish()) {
            LOG_INFO << "Successfully parsed http request\n";
            if (m_onRequest) {
                m_onRequest(shared_from_this());
            }
            m_request.Reset();
        } else {
            LOG_INFO << "Http request is incomplete, wait left data...\n";
        }
        m_readBuffer->Retrieve(parsed); // 支持半包和粘包请求，解析完一部分就从缓冲区删除掉，继续读取剩余部分
    }
}

void HttpConnect::HandleError(int errorNum, std::string errorMsg) {
    if (m_onError) {
        m_onError(shared_from_this());
        return;
    }
    std::string body_buff;
    std::string header_buff;
    body_buff = ErrorPage(errorNum, errorMsg);

    header_buff += "HTTP/1.1 " + std::to_string(errorNum) + " " + errorMsg + "\r\n";
    header_buff += "Content-Type: text/html; charset=utf-8\r\n";
    header_buff += "Connection: Close\r\n";
    header_buff += "Content-Length: " + std::to_string(body_buff.size()) + "\r\n";
    header_buff += "Server: Moran's Web Server\r\n";
    header_buff += "\r\n";
    std::string response = header_buff + body_buff;
    m_closeAfterWrite = true;
    Send(response);
}

void HttpConnect::SeperateTimer() {
    if (auto timer = m_timer.lock()) {
        timer->ClearCallback(); // 分离定时器，清除定时器的回调函数，避免定时器过期时错误地关闭连接
        m_timer.reset();
    }
}

void HttpConnect::Send(const std::string& response) {
    Send(response.c_str(), static_cast<int>(response.size()));
}

void HttpConnect::Send(const char* response, int len) {
    m_writeBuffer->Append(response, len);
    WriteNonBlocking();
}

void HttpConnect::Send(const char* response) {
    Send(response, static_cast<int>(strlen(response)));
}

void HttpConnect::ReadNonBlocking() {
    int sockfd = m_connectedFd;
    char buf[READ_BUFFER]{0};
    while (true) {
        // std::fill(buf, buf + READ_BUFFER, 0);
        // memset(buf, 0, READ_BUFFER); // 性能更优->不需要置0
        auto byte_read = read(sockfd, buf, READ_BUFFER);
        if (byte_read > 0) {
            // cout << "Get messages from " << sockfd << ":" << buf << '\n';
            LOG_INFO << "Get messages from " << sockfd << ":" << buf << '\n';
            m_readBuffer->Append(buf, byte_read);
        } else if (byte_read == -1 && errno == EINTR) {
            // 客户端正常中断
            cout << "continue reading..." << '\n';
            continue;
        } else if (byte_read == -1 && ((errno == EAGAIN) || (errno == EWOULDBLOCK))) {
            // 非阻塞IO表示数据全部读取完毕
            LOG_INFO << "Finish reading once" << '\n';
            break;
        } else if (byte_read == 0) { // EOF,客户端断开连接
            LOG_ERROR << "EOF, client " << sockfd << " disconnected!" << '\n';
            HandleClose();
            break;
        } else {
            // cout << "Other error on client fd " << sockfd << '\n';
            LOG_ERROR << "Other error on client fd " << sockfd << '\n';
            m_state = State::Closed;
            break;
        }
    }
}

void HttpConnect::WriteNonBlocking() {
    while (m_writeBuffer->ReadableBytes() > 0) {
        auto sendSzie = static_cast<int>(write(m_connectedFd, m_writeBuffer->Peek(), m_writeBuffer->ReadableBytes()));
        if (sendSzie > 0) {
            m_writeBuffer->Retrieve(sendSzie);
            continue;
        }
        if (sendSzie == -1 && (errno == EINTR)) {
            continue;
        }
        if (sendSzie == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            m_channel->EnableWrite();
            return;
        }
        LOG_ERROR << "Error on write!";
        HandleClose(); // 写操作遇到未知错误，关闭连接
        return;
    }
    // 写完后关闭，否则 socket 通常一直可写，会导致 epoll 持续唤醒，CPU 空转
    m_channel->DisableWrite();
    if (m_closeAfterWrite) {
        HandleClose();
    }
}
