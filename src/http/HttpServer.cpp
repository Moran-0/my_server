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
#include <vector>
#include <algorithm>

using std::cout;
using std::endl;
using Status = HttpResponse::HttpStatusCode;

namespace {
constexpr std::uintmax_t kMaxStaticFileSize = 32 * 1024 * 1024;

std::string GetMimeType(const std::string& suffix);

struct FileEntry {
    enum class Type : std::uint8_t { FILE, DIRECTORY };
    std::string name;
    std::string relativePath;
    std::uintmax_t size;
    Type type;
};

/// @brief 文件大小格式化
/// @param size
/// @return
std::string FormatFileSize(std::uintmax_t size) {
    const char* units[] = {"B", "KB", "MB", "GB"};
    auto value = static_cast<double>(size);
    int unit = 0;

    while (value >= 1024.0 && unit < 3) {
        value /= 1024.0;
        ++unit;
    }

    std::ostringstream out;
    if (unit == 0) {
        out << static_cast<std::uintmax_t>(value) << ' ' << units[unit];
    } else {
        out.setf(std::ios::fixed);
        out.precision(1);
        out << value << ' ' << units[unit];
    }
    return out.str();
}

/// @brief 对字符串进行URL编码
/// @param text
/// @return
std::string UrlEncode(const std::string& text) {
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string encoded;

    for (unsigned char ch : text) {
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' || ch == '.' || ch == '~') {
            encoded.push_back(static_cast<char>(ch));
        } else {
            encoded.push_back('%');
            encoded.push_back(hex[ch >> 4]);
            encoded.push_back(hex[ch & 0x0F]);
        }
    }

    return encoded;
}

bool IsSafePath(const std::string& value) {
    // std::string 可以存储任意字符序列（包括 '\0'）,需要进行排除
    if (value.find("..") != std::string::npos || value.find('\0') != std::string::npos) {
        return false;
    }
    std::filesystem::path path(value);
    if (path.is_absolute()) {
        return false;
    }
    return true;
}

bool ResolveUserPath(const std::string& staticRoot, const std::string& username, const std::string& path, std::filesystem::path& resolvePath) {
    if (username.empty() || username.find("..") != std::string::npos || username.find('/') != std::string::npos ||
        username.find('\\') != std::string::npos || !IsSafePath(path)) {
        return false;
    }

    std::error_code ec;
    const auto staticRootPath = std::filesystem::weakly_canonical(std::filesystem::path(staticRoot), ec);
    if (ec) {
        return false;
    }
    const auto userRootPath = std::filesystem::weakly_canonical(staticRootPath / username, ec);
    if (ec || !std::filesystem::exists(userRootPath, ec) || !std::filesystem::is_directory(userRootPath, ec)) {
        return false;
    }

    resolvePath = std::filesystem::weakly_canonical(userRootPath / path, ec);
    if (ec || !std::filesystem::exists(resolvePath, ec)) {
        return false;
    }

    const auto relative = std::filesystem::relative(resolvePath, staticRootPath, ec);
    if (ec || relative.empty()) {
        return false;
    }
    return true;
}

/// @brief 读取用户目录文件和文件夹列表
/// @param staticRoot
/// @param username 用户名
/// @return
std::vector<FileEntry> LoadUserContent(const std::string& staticRoot, const std::string& username, const std::string& curDir) {
    std::vector<FileEntry> entries;
    std::filesystem::path userDir;

    std::error_code ec;
    if (!ResolveUserPath(staticRoot, username, curDir, userDir)) {
        return entries;
    }
    const auto userRoot = std::filesystem::weakly_canonical(std::filesystem::path(staticRoot) / username, ec);
    if (ec) {
        return entries;
    }

    for (const auto& entry : std::filesystem::directory_iterator(userDir, ec)) {
        if (ec) {
            break;
        }

        const auto& path = entry.path();
        const auto name = path.filename().string();
        const auto rel = std::filesystem::relative(path, userRoot, ec);
        if (ec) {
            continue;
        }
        if (entry.is_regular_file()) {
            const auto size = entry.file_size(ec);
            if (ec) {
                continue;
            }
            entries.push_back(FileEntry{name, rel.string(), size, FileEntry::Type::FILE});

        } else if (entry.is_directory()) {
            entries.push_back(FileEntry{name, rel.string(), 0, FileEntry::Type::DIRECTORY});
        }
    }

    std::sort(entries.begin(), entries.end(), [](const FileEntry& lhs, const FileEntry& rhs) {
        if (lhs.type != rhs.type) {
            return lhs.type == FileEntry::Type::DIRECTORY;
        }
        return lhs.name < rhs.name;
    });

    return entries;
}

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

std::unordered_map<std::string, std::string> ParseQueryString(const std::string& url) {
    std::unordered_map<std::string, std::string> result;

    const auto q = url.find('?');
    if (q == std::string::npos || q + 1 >= url.size()) {
        return result;
    }

    return ParseFormUrlEncoded(url.substr(q + 1));
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
        if (ch == '\r' || ch == '\n' || ch == '\t' || ch == '"') {
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

std::string ParentDirOf(const std::string& currentDir) {
    if (currentDir.empty()) {
        return "";
    }

    std::filesystem::path p(currentDir);
    return p.parent_path().string();
}
std::string ContentListPage(const std::string& username, const std::string& currentDir, const std::vector<FileEntry>& entries,
                            const std::string& notice = "") {
    std::string rows;
    std::string noticeBlock;

    const auto encodedUser = UrlEncode(username);
    const auto encodedDir = UrlEncode(currentDir);

    if (!notice.empty()) {
        noticeBlock = R"(
      <div class="notice">)" +
                      HtmlEscape(notice) + R"(</div>
        )";
    }

    if (!currentDir.empty()) {
        const auto parent = ParentDirOf(currentDir);
        rows += R"(
          <a class="entry up" href="/files?username=)" +
                encodedUser + R"(&dir=)" + UrlEncode(parent) + R"(">
            <span class="icon">UP</span>
            <span class="name">..</span>
            <span class="size">Parent directory</span>
            <span></span>
            <span></span>
          </a>
        )";
    }

    if (entries.empty()) {
        rows += R"(
          <div class="empty">
            <strong>No entries</strong>
            <span>This directory is empty.</span>
          </div>
        )";
    } else {
        for (const auto& entry : entries) {
            if (entry.type == FileEntry::Type::DIRECTORY) {
                rows += R"(
          <a class="entry" href="/files?username=)" +
                        encodedUser + R"(&dir=)" + UrlEncode(entry.relativePath) + R"(">
            <span class="icon folder">DIR</span>
            <span class="name">)" +
                        HtmlEscape(entry.name) + R"(</span>
            <span class="size">Directory</span>
            <span></span>
            <span></span>
          </a>
                )";
            } else {
                rows += R"(
          <div class="entry">
            <span class="icon">FILE</span>
            <a class="name" href="/)" +
                        HtmlEscape(UrlEncode(username)) + "/" + HtmlEscape(entry.relativePath) + R"(" target="_blank" rel="noopener">)" +
                        HtmlEscape(entry.name) + R"(</a>
            <span class="size">)" +
                        FormatFileSize(entry.size) + R"(</span>

            <form method="post" action="/download">
              <input type="hidden" name="username" value=")" +
                        HtmlEscape(username) + R"(">
              <input type="hidden" name="path" value=")" +
                        HtmlEscape(entry.relativePath) + R"(">
              <button class="download" type="submit">Download</button>
            </form>

            <form method="post" action="/delete">
              <input type="hidden" name="username" value=")" +
                        HtmlEscape(username) + R"(">
              <input type="hidden" name="path" value=")" +
                        HtmlEscape(entry.relativePath) + R"(">
              <button class="delete" type="submit">Delete</button>
            </form>
          </div>
                )";
            }
        }
    }

    return R"(<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>)" +
           HtmlEscape(username) + R"('s Files</title>
  <style>
    :root {
      color-scheme: light;
      --bg: #f5f7fb;
      --panel: #ffffff;
      --text: #182230;
      --muted: #667085;
      --line: #d8dee9;
      --accent: #2563eb;
      --soft: #eef4ff;
      --danger: #dc2626;
      --danger-soft: #fef2f2;
      --ok-soft: #ecfdf3;
      --ok-text: #067647;
      --folder: #7c3aed;
    }
    * {
      box-sizing: border-box;
    }
    body {
      margin: 0;
      min-height: 100vh;
      padding: 36px 24px;
      font-family: ui-sans-serif, system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
      background: var(--bg);
      color: var(--text);
    }
    main {
      width: min(1040px, 100%);
      margin: 0 auto;
    }
    header {
      display: flex;
      justify-content: space-between;
      gap: 20px;
      align-items: flex-end;
      margin-bottom: 22px;
    }
    h1 {
      margin: 0;
      font-size: clamp(34px, 6vw, 58px);
      line-height: 1;
      letter-spacing: 0;
    }
    p {
      margin: 12px 0 0;
      color: var(--muted);
      line-height: 1.6;
    }
    code {
      padding: 2px 6px;
      border-radius: 6px;
      background: #eef2f7;
      color: var(--text);
    }
    .home {
      flex: 0 0 auto;
      padding: 10px 14px;
      border: 1px solid var(--line);
      border-radius: 8px;
      color: var(--text);
      background: var(--panel);
      font-weight: 700;
      text-decoration: none;
    }
    .panel {
      border: 1px solid var(--line);
      border-radius: 8px;
      background: var(--panel);
      box-shadow: 0 18px 42px rgba(24, 34, 48, 0.08);
      overflow: hidden;
    }
    .notice {
      margin: 16px;
      padding: 12px 14px;
      border: 1px solid #abefc6;
      border-radius: 8px;
      background: var(--ok-soft);
      color: var(--ok-text);
      font-weight: 700;
    }
    .toolbar {
      display: flex;
      justify-content: space-between;
      gap: 12px;
      padding: 16px 18px;
      border-bottom: 1px solid var(--line);
      background: #fbfcfe;
      color: var(--muted);
      font-size: 14px;
      font-weight: 700;
    }
    .list {
      display: grid;
    }
    .entry {
      display: grid;
      grid-template-columns: 72px minmax(0, 1fr) 130px 110px 90px;
      gap: 14px;
      align-items: center;
      min-height: 64px;
      padding: 14px 18px;
      border-bottom: 1px solid var(--line);
      color: var(--text);
      text-decoration: none;
    }
    .entry:last-child {
      border-bottom: 0;
    }
    .entry:hover {
      background: var(--soft);
    }
    .entry form {
      margin: 0;
    }
    .entry button {
      width: 100%;
      min-height: 38px;
      border: 1px solid var(--line);
      border-radius: 8px;
      background: var(--panel);
      color: var(--text);
      font: inherit;
      font-size: 14px;
      font-weight: 800;
      cursor: pointer;
    }
    .entry button:hover {
      border-color: var(--accent);
      color: var(--accent);
    }
    .entry .delete {
      border-color: #fecaca;
      background: var(--danger-soft);
      color: var(--danger);
    }
    .entry .delete:hover {
      border-color: var(--danger);
    }
    .icon {
      display: inline-grid;
      place-items: center;
      width: 52px;
      height: 34px;
      border-radius: 6px;
      background: var(--accent);
      color: #ffffff;
      font-size: 11px;
      font-weight: 800;
    }
    .folder {
      background: var(--folder);
    }
    .name {
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: nowrap;
      color: var(--text);
      font-weight: 700;
      text-decoration: none;
    }
    .name:hover {
      color: var(--accent);
      text-decoration: underline;
    }
    .size {
      color: var(--muted);
      text-align: right;
      font-size: 14px;
    }
    .empty {
      display: grid;
      gap: 8px;
      padding: 42px 20px;
      text-align: center;
    }
    .empty strong {
      font-size: 22px;
    }
    .empty span {
      color: var(--muted);
    }
    @media (max-width: 760px) {
      body {
        padding: 24px 16px;
      }
      header {
        display: grid;
        align-items: start;
      }
      .home {
        width: fit-content;
      }
      .entry {
        grid-template-columns: 52px minmax(0, 1fr);
      }
      .size,
      .entry form {
        grid-column: 2;
        text-align: left;
      }
    }
  </style>
</head>
<body>
  <main>
    <header>
      <div>
        <h1>Your files</h1>
        <p>Signed in as <strong>)" +
           HtmlEscape(username) + R"(</strong>. Current directory: <code>/)" + HtmlEscape(currentDir) + R"(</code></p>
      </div>
      <a class="home" href="/">Back home</a>
    </header>

    <section class="panel">
)" + noticeBlock +
           R"(
      <div class="toolbar">
        <span>Name</span>
        <span>)" +
           std::to_string(entries.size()) + R"( item(s)</span>
      </div>
      <div class="list">
)" + rows + R"(
      </div>
    </section>
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

void SendDownloadResponse(const std::shared_ptr<HttpConnect>& conn, HttpResponse& response, const std::filesystem::path& filePath,
                          const std::string& filename, bool closeConnection) {
    std::ifstream file(filePath, std::ios::in | std::ios::binary);
    if (!file) {
        response.SetStatusCode(Status::K403Forbidden);
        response.SetStatusMessage("Forbidden");
        SendHtmlResponse(conn, response, ResultPage("Download failed", "The server could not open the selected file."), closeConnection);
        return;
    }

    std::ostringstream body;
    body << file.rdbuf();

    response.SetStatusCode(Status::K200K);
    response.SetStatusMessage("OK");
    response.SetContentType(GetMimeType(filePath.extension().string()));
    response.AddHeader("Content-Disposition", "attachment; filename=\"" + RecordValue(filename) + "\"");
    response.SetBody(body.str());

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
    if (path != "/login" && path != "/register" && path != "/download" && path != "/delete") {
        response.SetStatusCode(Status::K404NotFound);
        response.SetStatusMessage("Not Found");
        SendHtmlResponse(conn, response, ErrorBody(Status::K404NotFound, "Not Found"), closeConnection);
        return true;
    }

    const auto form = ParseFormUrlEncoded(request.GetBody());
    const auto username = form.find("username");
    std::string currentDir{};

    if (username == form.end() || username->second.empty()) {
        response.SetStatusCode(Status::K400BadRequest);
        response.SetStatusMessage("Bad Request");
        SendHtmlResponse(conn, response, ResultPage("Missing information", "Please provide a username."), closeConnection);
        return true;
    }

    if (path == "/download" || path == "/delete") {
        const auto filename = form.find("path");
        std::filesystem::path filePath;

        if (filename == form.end() || !ResolveUserPath(m_staticRoot, username->second, filename->second, filePath)) {
            response.SetStatusCode(Status::K404NotFound);
            response.SetStatusMessage("Not Found");
            SendHtmlResponse(conn, response, ResultPage("File not found", "The requested file does not exist or cannot be accessed."),
                             closeConnection);
            return true;
        }

        if (path == "/download") {
            SendDownloadResponse(conn, response, filePath, filename->second, closeConnection);
            return true;
        }

        std::error_code ec;
        std::filesystem::remove(filePath, ec);
        const auto userRoot = std::filesystem::weakly_canonical(std::filesystem::path(m_staticRoot) / username->second);
        currentDir = std::filesystem::relative(filePath.parent_path(), userRoot).string();
        response.SetStatusCode(ec ? Status::K500internalServerError : Status::K200K);
        response.SetStatusMessage(ec ? "Internal Server Error" : "OK");
        const auto files = LoadUserContent(m_staticRoot, username->second, currentDir);
        const auto notice = ec ? "Delete failed. Please check file permissions." : "Deleted \"" + filename->second + "\".";
        SendHtmlResponse(conn, response, ContentListPage(username->second, currentDir, files, notice), closeConnection);
        return true;
    }

    const auto password = form.find("password");
    if (password == form.end() || password->second.empty()) {
        response.SetStatusCode(Status::K400BadRequest);
        response.SetStatusMessage("Bad Request");
        SendHtmlResponse(conn, response, ResultPage("Missing information", "Please provide both username and password."), closeConnection);
        return true;
    }

    if (path == "/login") {
        response.SetStatusCode(Status::K200K);
        response.SetStatusMessage("OK");

        if (password->second == "password") {
            const auto files = LoadUserContent(m_staticRoot, username->second, currentDir);
            SendHtmlResponse(conn, response, ContentListPage(username->second, currentDir, files), closeConnection);
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

    if (path == "/files") {
        const auto query = ParseQueryString(request.GetUrl());
        const auto username = query.find("username");
        const auto dir = query.find("dir");

        if (username == query.end() || username->second.empty()) {
            response.SetStatusCode(Status::K400BadRequest);
            response.SetStatusMessage("Bad Request");
            SendHtmlResponse(conn, response, ResultPage("Missing username", "The file list request needs a username."), closeConnection);
            return true;
        }
        const auto currentDir = dir == query.end() ? "" : dir->second;
        const auto files = LoadUserContent(m_staticRoot, username->second, currentDir);
        SendHtmlResponse(conn, response, ContentListPage(username->second, currentDir, files), closeConnection);
        return true;
    }

    std::error_code ec;
    if (!std::filesystem::exists(filePath, ec)) {
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
