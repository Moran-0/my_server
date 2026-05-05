#include "HttpServer.h"
#include "Socket.h"
#include "EventLoop.h"
#include "InetAddress.h"
#include "Channel.h"
#include "Acceptor.h"
#include "Buffer.h"
#include "ThreadPool.h"
#include "HttpResponse.h"
#include "HttpConfig.h"
#include "Logging.h"

#include <iostream>
#include <fcntl.h>
#include <cerrno>
#include <unistd.h>
#include <functional>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

using std::cout;
using std::endl;
using Status = HttpResponse::HttpStatusCode;

namespace {
constexpr std::uintmax_t kMaxStaticFileSize = 32 * 1024 * 1024;

std::string DecodeUrlPath(const std::string& url) {
    std::string path;
    path.reserve(url.size());
    const auto queryPos = url.find('?');
    const auto end = queryPos == std::string::npos ? url.size() : queryPos;

    for (size_t i = 0; i < end; ++i) {
        if (url[i] == '%' && i + 2 < end) {
            const auto hex = url.substr(i + 1, 2);
            char* parseEnd = nullptr;
            const long value = std::strtol(hex.c_str(), &parseEnd, 16);
            if (parseEnd == hex.c_str() + 2) {
                path.push_back(static_cast<char>(value));
                i += 2;
                continue;
            }
        }
        path.push_back(url[i]);
    }

    return path;
}
std::string ErrorBody(Status statusCode, const std::string& statusMessage) {
    const auto code = std::to_string(statusCode);
    std::string body = R"(<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>)" + code + " " + statusMessage + R"(</title>
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
    <h1>)" + statusMessage + R"(</h1>
    <p>The server understood the request, but could not complete it in its current form.</p>
    <a href="/">Return home</a>
  </main>
</body>
</html>)";
    return body;
}

const std::unordered_map<std::string, std::string> MIME_TYPE = {
    {".html", "text/html"},          {".xml", "text/xml"},          {".xhtml", "application/xhtml+xml"},
    {".txt", "text/plain"},          {".rtf", "application/rtf"},   {".pdf", "application/pdf"},
    {".word", "application/nsword"}, {".png", "image/png"},         {".gif", "image/gif"},
    {".jpg", "image/jpeg"},          {".jpeg", "image/jpeg"},       {".au", "audio/basic"},
    {".mpeg", "video/mpeg"},         {".mpg", "video/mpeg"},        {".avi", "video/x-msvideo"},
    {".gz", "application/x-gzip"},   {".tar", "application/x-tar"}, {".css", "text/css "},
    {".js", "text/javascript "},     {"default", "text/html"}};
std::string GetMimeType(const std::string& suffix) {
    auto it = MIME_TYPE.find(suffix);
    if (it == MIME_TYPE.end()) {
        return MIME_TYPE.at("default");
    }
    return it->second;
}
} // namespace

HttpServer::HttpServer(const char* ip, int port) : m_staticRoot("./static") {
    m_mainReactor = std::make_unique<EventLoop>();
    m_acceptor = std::make_unique<Acceptor>(m_mainReactor.get(), ip, port);
    m_acceptor->SetNewConnectionCallback([this](int sock_fd) { this->CreateConnection(sock_fd); });
    int size = std::thread::hardware_concurrency();
    m_subReactorsPool = std::make_unique<EventLoopThreadPool>(m_mainReactor.get(), size);
    m_requestCallback = [this](const std::shared_ptr<HttpConnect>& conn) { this->OnRequest(conn); };
}

void HttpServer::start() {
    m_subReactorsPool->Start(); // 创建并启动线程池，创建subReactor线程，并让每个subReactor线程的EventLoop开始事件循环
    LOG_INFO << "HttpServer " << " Start!" << '\n';
    m_mainReactor->loop();
}

void HttpServer::CloseConnection(const std::shared_ptr<HttpConnect>& conn) {
    LOG_INFO << "[CloseConnection] Close connection in thread " << CurrentThread::Tid() << '\n';
    m_mainReactor->RunOneFunc([this, conn]() { this->CloseConnectionInLoop(conn); });
}

/// @brief 在loop当前线程中关闭连接，移除连接对象，并将该连接所属channel从EventLoop中移除。由于loop所属线程唯一，因此不需要加锁保护m_connections
/// @param conn 连接对象
void HttpServer::CloseConnectionInLoop(const std::shared_ptr<HttpConnect>& conn) {
    LOG_INFO << "[CloseConnectionInLoop] Close connection in thread " << CurrentThread::Tid() << '\n';
    m_connections.erase(conn->GetFd());
    // 将该连接所属channel从EventLoop中移除
    auto* conn_loop = conn->GetLoop();
    conn_loop->QueueFunc([conn_loop, conn]() {
        conn->DestroyConnection();
    }); // 当前在mainReactor线程中，一定不在conn所属的EventLoop线程中，因此需要通过QueueFunc将DestroyConnection函数添加到conn所属EventLoop的任务队列中，由conn所属EventLoop所在的线程来执行DestroyConnection函数，从而安全地移除连接对象和channel对象
}

void HttpServer::CreateConnection(int sock_fd) {
    if (sock_fd == -1) {
        throw std::runtime_error("无效套接字");
    }
    auto* work_loop = m_subReactorsPool->GetNextLoop();
    auto connect = std::make_shared<HttpConnect>(work_loop, sock_fd);
    connect->SetOnCloseCallback([this](const std::shared_ptr<HttpConnect>& conn) { this->CloseConnection(conn); });
    connect->SetOnRequestCallback(m_requestCallback);
    connect->SetOnConnectCallback(m_connectCallback);
    {
        std::lock_guard<std::mutex> lock(m_connections_mtx);
        m_connections[sock_fd] = connect;
    }
    work_loop->RunOneFunc([work_loop, connect]() {
        connect->EstablishConnection();
        work_loop->AddScheduledTask(
            HTTP_DEFAULT_TIMEOUT, [connect]() { connect->HandleClose(); },
            [connect](const std::shared_ptr<Timer>& timer) { connect->LinkTimer(timer); });
    });
}

void HttpServer::OnRequest(const std::shared_ptr<HttpConnect>& conn) {
    ServeStaticFile(conn);
}

bool HttpServer::ServeStaticFile(const std::shared_ptr<HttpConnect>& conn) {
    auto request = conn->GetRequest();
    const bool closeConnection = !request.IsKeepAlive();
    HttpResponse response(closeConnection);
    response.AddHeader("Server", "Moran's Web Server");

    const auto method = request.GetMethod();
    if (method != HttpRequest::HttpMethod::GET && method != HttpRequest::HttpMethod::HEAD) {
        response.SetStatusCode(Status::K405MethodNotAllowed);
        response.SetStatusMessage("Method Not Allowed");
        response.SetContentType("text/html; charset=utf-8");
        response.AddHeader("Allow", "GET, HEAD");
        response.SetBody(ErrorBody(Status::K405MethodNotAllowed, "Method Not Allowed"));
        if (closeConnection) {
            conn->SetCloseAfterWrite(true);
        }
        conn->Send(response.message());
        return true;
    }

    auto path = DecodeUrlPath(request.GetUrl());
    if (path.empty() || path[0] != '/') {
        path = "/";
    }
    if (path.find('\0') != std::string::npos || path.find("..") != std::string::npos) {
        response.SetStatusCode(HttpResponse::K403Forbidden);
        response.SetStatusMessage("Forbidden");
        response.SetContentType("text/html; charset=utf-8");
        response.SetBody(ErrorBody(Status::K403Forbidden, "Forbidden"));
        if (closeConnection) {
            conn->SetCloseAfterWrite(true);
        }
        conn->Send(response.message());
        return true;
    }

    std::filesystem::path filePath = std::filesystem::path(m_staticRoot) / path.substr(1);
    if (path.back() == '/') {
        filePath /= "index.html";
    }

    std::error_code ec;
    if (!std::filesystem::exists(filePath, ec) || !std::filesystem::is_regular_file(filePath, ec)) {
        response.SetStatusCode(HttpResponse::K404NotFound);
        response.SetStatusMessage("Not Found");
        response.SetContentType("text/html; charset=utf-8");
        response.SetBody(ErrorBody(Status::K404NotFound, "Not Found"));
        if (closeConnection) {
            conn->SetCloseAfterWrite(true);
        }
        conn->Send(response.message());
        return true;
    }

    const auto fileSize = std::filesystem::file_size(filePath, ec);
    if (ec || fileSize > kMaxStaticFileSize) {
        response.SetStatusCode(HttpResponse::K413PayloadTooLarge);
        response.SetStatusMessage("Payload Too Large");
        response.SetContentType("text/html; charset=utf-8");
        response.SetBody(ErrorBody(Status::K413PayloadTooLarge, "Payload Too Large"));
        if (closeConnection) {
            conn->SetCloseAfterWrite(true);
        }
        conn->Send(response.message());
        return true;
    }

    std::ifstream file(filePath, std::ios::in | std::ios::binary);
    if (!file) {
        response.SetStatusCode(HttpResponse::K403Forbidden);
        response.SetStatusMessage("Forbidden");
        response.SetContentType("text/html; charset=utf-8");
        response.SetBody(ErrorBody(Status::K403Forbidden, "Forbidden"));
        if (closeConnection) {
            conn->SetCloseAfterWrite(true);
        }
        conn->Send(response.message());
        return true;
    }

    std::ostringstream body;
    body << file.rdbuf();

    response.SetStatusCode(HttpResponse::K200K);
    response.SetStatusMessage("OK");
    response.SetContentType(GetMimeType(filePath.extension().string()));
    // response.AddHeader("Content-Length", std::to_string(fileSize));
    response.SetBody(method == HttpRequest::HttpMethod::HEAD ? "" : body.str());

    if (closeConnection) {
        conn->SetCloseAfterWrite(true);
    }
    conn->Send(response.message());
    return true;
}
