#include "HttpRequest.h"

constexpr char CRLF[] = "\r\n";

HttpRequest::HttpRequest()
    : m_method(HttpMethod::INVALID), m_version(HttpVersion::UNKNOWN), m_parseState(HttpParseState::REQUEST_LINE), m_lineState(LineState::METHOD),
      m_headerState(HeaderState::KEY), m_contentLength(0), m_isKeepAlive(false) {}

int HttpRequest::Parse(const std::string& buffer) {
    if (buffer.empty()) {
        return -1;
    }
    const char* ptr = buffer.c_str();
    m_start = m_parseStart = m_end = ptr;
    for (size_t i = 0; i < buffer.size(); ++i) {
        char c = buffer[i];
        m_curPos = ptr + i;
        switch (m_parseState) {
        case HttpParseState::REQUEST_LINE:
            ParseRequestLine(c);
            break;
        case HttpParseState::HEADERS:
            ParseHeader(c);
            break;
        case HttpParseState::BODY:
            ParseBody(c);
            break;
        case HttpParseState::ERROR:
            return -1;
        default:
            break;
        }
    }
    return m_end - m_start;
}

void HttpRequest::SetMethod(const std::string_view& method) {
    if (method == "GET") {
        m_method = HttpMethod::GET;
    } else if (method == "POST") {
        m_method = HttpMethod::POST;
    } else if (method == "HEAD") {
        m_method = HttpMethod::HEAD;
    } else if (method == "PUT") {
        m_method = HttpMethod::PUT;
    } else if (method == "DELETE") {
        m_method = HttpMethod::DELETE;
    } else if (method == "CONNECT") {
        m_method = HttpMethod::CONNECT;
    } else if (method == "OPTIONS") {
        m_method = HttpMethod::OPTIONS;
    } else if (method == "TRACE") {
        m_method = HttpMethod::TRACE;
    } else if (method == "PATCH") {
        m_method = HttpMethod::PATCH;
    } else {
        m_method = HttpMethod::INVALID;
    }
}

std::string HttpRequest::GetMethodString() const {
    switch (m_method) {
    case HttpMethod::GET:
        return "GET";
    case HttpMethod::POST:
        return "POST";
    case HttpMethod::HEAD:
        return "HEAD";
    case HttpMethod::PUT:
        return "PUT";
    case HttpMethod::DELETE:
        return "DELETE";
    case HttpMethod::CONNECT:
        return "CONNECT";
    case HttpMethod::OPTIONS:
        return "OPTIONS";
    case HttpMethod::TRACE:
        return "TRACE";
    case HttpMethod::PATCH:
        return "PATCH";
    default:
        return "INVALID";
    }
}

void HttpRequest::SetVersion(const std::string_view& version) {
    if (version == "HTTP/1.0") {
        m_version = HttpVersion::HTTP_10;
    } else if (version == "HTTP/1.1") {
        m_version = HttpVersion::HTTP_11;
    } else {
        m_version = HttpVersion::UNKNOWN;
    }
}

std::string HttpRequest::GetVersionString() const {
    switch (m_version) {
    case HttpVersion::HTTP_10:
        return "HTTP/1.0";
    case HttpVersion::HTTP_11:
        return "HTTP/1.1";
    default:
        return "UNKNOWN";
    }
}

void HttpRequest::SetRequestParameters(std::unordered_map<std::string, std::string> params) {
    m_requestParameters = std::move(params);
}

void HttpRequest::SetRequestParameter(const std::string& key, const std::string& value) {
    m_requestParameters[key] = value;
}

void HttpRequest::AddHeader(const std::string& key, const std::string& value) {
    m_headers[key] = value;
}

std::string HttpRequest::GetHeader(const std::string& key) const {
    auto it = m_headers.find(key);
    if (it != m_headers.end()) {
        return it->second;
    }
    return "";
}
/// @brief 重置HttpRequest对象的状态和成员变量，以便处理新的HTTP请求
void HttpRequest::Reset() {
    m_method = HttpMethod::INVALID;
    m_version = HttpVersion::UNKNOWN;
    m_url.clear();
    m_requestParameters.clear();
    m_protocol.clear();
    m_headers.clear();
    m_body.clear();
    m_contentLength = 0;
    m_postParameters.clear();
    m_parseState = HttpParseState::REQUEST_LINE;
    m_lineState = LineState::METHOD;
    m_headerState = HeaderState::KEY;
    m_parseStart = m_curPos = m_start = m_end = nullptr;
}

void HttpRequest::ParseRequestLine(const char c) {
    switch (m_lineState) {
    case LineState::METHOD:
        if (c == ' ') {
            std::string_view method(m_parseStart, m_curPos - m_parseStart);
            SetMethod(method);
            m_lineState = LineState::URL;
            m_parseStart = m_curPos + 1;
        }
        break;
    case LineState::URL:
        if (c == ' ') {
            std::string_view url(m_parseStart, m_curPos - m_parseStart);
            SetUrl(url);
            m_lineState = LineState::VERSION;
            m_parseStart = m_curPos + 1;
        }
        break;
    case LineState::VERSION:
        if (c == '\r') {
            std::string_view version(m_parseStart, m_curPos - m_parseStart);
            SetVersion(version);
            if (m_version == HttpVersion::HTTP_11) {
                m_isKeepAlive = true; // HTTP/1.1默认使用长连接
            }
            m_lineState = LineState::LINE_END;
        }
        break;
    case LineState::LINE_END:
        if (c == '\n') {
            m_parseState = HttpParseState::HEADERS;
            m_headerState = HeaderState::KEY;
            m_end = m_curPos + 1;
            m_parseStart = m_curPos + 1;
        } else {
            m_parseState = HttpParseState::ERROR;
        }
        break;
    default:
        break;
    }
}

void HttpRequest::ParseHeader(const char c) {
    static std::string curKey;
    static std::string curValue;
    switch (m_headerState) {
    case HeaderState::KEY:
        if (c == ':') {
            m_headerState = HeaderState::COLON;
            curKey = std::string(m_parseStart, m_curPos - m_parseStart);
            m_parseStart = m_curPos + 1;
        } else if (c == '\r' || c == '\n') {
            m_parseState = HttpParseState::ERROR;
        }
        break;
    case HeaderState::COLON:
        if (c == ' ') {
            m_headerState = HeaderState::SPACE_AFTER_COLON;
        } else {
            m_parseState = HttpParseState::ERROR;
        }
        break;
    case HeaderState::SPACE_AFTER_COLON:
        m_headerState = HeaderState::VALUE;
        m_parseStart = m_curPos;
        break;
    case HeaderState::VALUE:
        if (c == '\r') {
            /**
             * 每个头部字段包含一个不区分大小写的字段名，后面跟一个冒号(":")、可选的前导空格(optional leading
             * whitespace)、字段值和可选的尾随空格。但是实际开发一般只有冒号后面有一个固定空格，所以遇到其他情况就直接当作错误处理了
             */
            curValue = std::string(m_parseStart, m_curPos - m_parseStart);
            if (curKey == "Content-Length") {
                m_contentLength = std::stoi(curValue);
            }
            AddHeader(curKey, curValue);
            if (curKey == "Connection" && curValue == "keep-alive") {
                m_isKeepAlive = true;
            }
            m_headerState = HeaderState::CR;
        }
        break;
    case HeaderState::CR:
        if (c == '\n') {
            m_headerState = HeaderState::LF;
        } else {
            m_parseState = HttpParseState::ERROR;
        }
        break;
    case HeaderState::LF:
        if (c == '\r') {
            m_headerState = HeaderState::END_CR;
        } else {
            m_headerState = HeaderState::KEY;
            m_parseStart = m_curPos;
        }
        break;
    case HeaderState::END_CR:
        if (c == '\n') {
            if (m_contentLength > 0) {
                m_parseState = HttpParseState::BODY;
            } else {
                m_parseState = HttpParseState::FINISH;
            }
            m_end = m_curPos + 1;
        } else {
            m_parseState = HttpParseState::ERROR;
        }
        break;
    default:
        break;
    }
}

void HttpRequest::ParseBody(const char c) {
    m_body.push_back(c);
    if (m_body.size() == static_cast<size_t>(m_contentLength)) {
        m_parseState = HttpParseState::FINISH;
        m_end = m_curPos + 1;
    }
}
