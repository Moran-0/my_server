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
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>

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
// url编码数据解码
std::string UrlDecode(const std::string& value) {
    std::string decoded;
    decoded.reserve(value.size());

    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '+') {
            decoded.push_back(' ');
            continue;
        }
        if (value[i] == '%' && i + 2 < value.size()) {
            const auto hex = value.substr(i + 1, 2);
            char* parseEnd = nullptr;
            const long ch = std::strtol(hex.c_str(), &parseEnd, 16);
            if (parseEnd == hex.c_str() + 2) {
                decoded.push_back(static_cast<char>(ch));
                i += 2;
                continue;
            }
        }
        decoded.push_back(value[i]);
    }

    return decoded;
}

std::unordered_map<std::string, std::string> ParseFormUrlEncoded(const std::string& body) {
    std::unordered_map<std::string, std::string> result;
    size_t pos = 0;

    while (pos <= body.size()) {
        const auto amp = body.find('&', pos);
        const auto end = amp == std::string::npos ? body.size() : amp;
        const auto eq = body.find('=', pos);

        if (eq != std::string::npos && eq < end) {
            result[UrlDecode(body.substr(pos, eq - pos))] = UrlDecode(body.substr(eq + 1, end - eq - 1));
        }

        if (amp == std::string::npos) {
            break;
        }
        pos = amp + 1;
    }

    return result;
}
/// @brief 避免响应页插入未转义用户输入
/// @param text
/// @return
std::string HtmlEscape(const std::string& text) {
    std::string escaped;
    escaped.reserve(text.size());

    for (char ch : text) {
        switch (ch) {
        case '&':
            escaped += "&amp;";
            break;
        case '<':
            escaped += "&lt;";
            break;
        case '>':
            escaped += "&gt;";
            break;
        case '"':
            escaped += "&quot;";
            break;
        case '\'':
            escaped += "&#39;";
            break;
        default:
            escaped.push_back(ch);
            break;
        }
    }

    return escaped;
}

std::string RecordValue(const std::string& text) {
    std::string value;
    value.reserve(text.size());

    for (char ch : text) {
        if (ch == '\r' || ch == '\n' || ch == '\t') {
            value.push_back(' ');
        } else {
            value.push_back(ch);
        }
    }

    return value;
}

std::string ResultPage(const std::string& title, const std::string& message) {
    return R"(<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>)" +
           HtmlEscape(title) + R"(</title>
  <style>
    :root {
      color-scheme: light;
      --bg: #f5f7fb;
      --panel: #ffffff;
      --text: #182230;
      --muted: #667085;
      --line: #d8dee9;
      --accent: #2563eb;
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
      font-family: ui-sans-serif, system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
      background: var(--bg);
      color: var(--text);
    }
    main {
      width: min(520px, 100%);
      padding: 34px;
      border: 1px solid var(--line);
      border-radius: 8px;
      background: var(--panel);
      box-shadow: 0 18px 42px rgba(24, 34, 48, 0.08);
    }
    h1 {
      margin: 0;
      font-size: 34px;
      line-height: 1.1;
      letter-spacing: 0;
    }
    p {
      margin: 16px 0 0;
      color: var(--muted);
      line-height: 1.7;
    }
    a {
      display: inline-block;
      margin-top: 24px;
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
    <h1>)" +
           HtmlEscape(title) + R"(</h1>
    <p>)" + HtmlEscape(message) +
           R"(</p>
    <a href="/">Back to home</a>
  </main>
</body>
</html>)";
}

bool AppendRegisteredUser(const std::unordered_map<std::string, std::string>& form) {
    std::filesystem::create_directories("data");
    std::ofstream out("data/users.txt", std::ios::out | std::ios::app);
    if (!out) {
        return false;
    }

    const auto now = static_cast<long long>(std::time(nullptr));
    out << "time=" << now << '\t' << "username=" << RecordValue(form.at("username")) << '\t' << "email=" << RecordValue(form.at("email")) << '\t'
        << "password=" << RecordValue(form.at("password")) << '\n';

    return true;
}

void SendHtmlResponse(const std::shared_ptr<HttpConnect>& conn, HttpResponse& response, const std::string& body, bool closeConnection) {
    response.SetContentType("text/html; charset=utf-8");
    response.SetBody(body);
    if (closeConnection) {
        conn->SetCloseAfterWrite(true);
    }
    conn->Send(response.message());
}
/// @brief 统一错误html返回
/// @param statusCode
/// @param statusMessage
/// @return
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
    const auto& request = conn->GetRequest();
    if (request.GetMethod() == HttpRequest::HttpMethod::POST) {
        HandlePostRequest(conn);
        return;
    }
    ServeStaticFile(conn);
}

bool HttpServer::HandlePostRequest(const std::shared_ptr<HttpConnect>& conn) {
    const auto request = conn->GetRequest();
    const bool closeConnection = !request.IsKeepAlive();
    HttpResponse response(closeConnection);
    response.AddHeader("Server", "Moran's Web Server");

    const auto path = DecodeUrlPath(request.GetUrl());
    if (path != "/login" && path != "/register") {
        response.SetStatusCode(Status::K404NotFound);
        response.SetStatusMessage("Not Found");
        SendHtmlResponse(conn, response, ErrorBody(Status::K404NotFound, "Not Found"), closeConnection);
        return true;
    }

    const auto form = ParseFormUrlEncoded(request.GetBody());
    const auto username = form.find("username");
    const auto password = form.find("password");

    if (username == form.end() || password == form.end() || username->second.empty() || password->second.empty()) {
        response.SetStatusCode(Status::K400BadRequest);
        response.SetStatusMessage("Bad Request");
        SendHtmlResponse(conn, response, ResultPage("Missing information", "Please provide both username and password."), closeConnection);
        return true;
    }

    if (path == "/login") {
        response.SetStatusCode(Status::K200K);
        response.SetStatusMessage("OK");

        if (password->second == "password") {
            SendHtmlResponse(
                conn, response,
                ResultPage("Welcome, " + username->second, "Demo login passed. The real SQL-backed verification can replace this branch later."),
                closeConnection);
        } else {
            SendHtmlResponse(conn, response, ResultPage("Login failed", "This demo accepts any username with password set to \"password\"."),
                             closeConnection);
        }
        return true;
    }

    const auto email = form.find("email");
    if (email == form.end() || email->second.empty()) {
        response.SetStatusCode(Status::K400BadRequest);
        response.SetStatusMessage("Bad Request");
        SendHtmlResponse(conn, response, ResultPage("Missing email", "Please provide an email address for registration."), closeConnection);
        return true;
    }

    if (!AppendRegisteredUser(form)) {
        response.SetStatusCode(Status::K500internalServerError);
        response.SetStatusMessage("Internal Server Error");
        SendHtmlResponse(conn, response, ResultPage("Registration failed", "The server could not write the registration record."), closeConnection);
        return true;
    }

    response.SetStatusCode(Status::K200K);
    response.SetStatusMessage("OK");
    SendHtmlResponse(conn, response, ResultPage("Registration saved", "The registration data was written to data/users.txt."), closeConnection);
    return true;
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
