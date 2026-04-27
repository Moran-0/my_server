#pragma once
#include <string>
#include <unordered_map>
#include <cstdint>
#include "Buffer.h"

class HttpRequest {
  public:
    enum class HttpMethod : std::int8_t { INVALID = -1, GET, POST, HEAD, PUT, DELETE, CONNECT, OPTIONS, TRACE, PATCH };
    enum class HttpVersion : std::int8_t { UNKNOWN = -1, HTTP_10, HTTP_11 };
    enum class HttpParseState : std::int8_t { REQUEST_LINE, HEADERS, BODY, FINISH, ERROR };                // http请求解析主状态
    enum class LineState : std::int8_t { METHOD, URL, VERSION, LINE_END };                                 // http请求解析行子状态
    enum class HeaderState : std::int8_t { KEY, COLON, SPACE_AFTER_COLON, VALUE, CR, LF, END_CR, END_LF }; // http请求解析头部子状态
    enum class HttpCode : std::int8_t {
        NO_REQUEST = 0,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE,
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION
    };

    HttpRequest();
    ~HttpRequest() = default;

    bool Parse(std::string& buffer);
    /**
     * 设置HTTP方法，支持GET、POST、HEAD、PUT、DELETE、CONNECT、OPTIONS、TRACE和PATCH方法，如果传入的method字符串不合法，则将HTTP方法设置为INVALID
     */

    HttpMethod GetMethod() const {
        return m_method;
    }
    std::string GetMethodString() const;

    /**
     * 设置HTTP版本，支持HTTP/1.0和HTTP/1.1版本，如果传入的version字符串不合法，则将HTTP版本设置为UNKNOWN
     */
    HttpVersion GetVersion() const {
        return m_version;
    }
    std::string GetVersionString() const;

    const std::string& GetUrl() const {
        return m_url;
    }

    const std::unordered_map<std::string, std::string>& GetRequestParameters() const {
        return m_requestParameters;
    };

    const std::string& GetProtocol() const {
        return m_protocol;
    }

    const std::unordered_map<std::string, std::string>& GetHeaders() const {
        return m_headers;
    };
    std::string GetHeader(const std::string& key) const;

    const std::string& GetBody() const {
        return m_body;
    }

  private:
    HttpMethod m_method;
    HttpVersion m_version;
    std::string m_url;
    std::unordered_map<std::string, std::string> m_requestParameters;
    std::string m_protocol;
    std::unordered_map<std::string, std::string> m_headers;
    std::string m_body;
    int m_contentLength;
    std::unordered_map<std::string, std::string> m_postParameters;
    HttpParseState m_parseState;
    LineState m_lineState;
    HeaderState m_headerState;
    const char* m_start;
    const char* m_curPos;

    void SetMethod(const std::string_view& method);
    void SetVersion(const std::string_view& version);
    // 设置请求参数，参数以键值对的形式存储在unordered_map中
    void SetRequestParameters(std::unordered_map<std::string, std::string> params);
    void SetRequestParameter(const std::string& key, const std::string& value);
    // 设置协议，协议以字符串的形式存储
    void SetProtocol(const std::string_view& protocol) {
        m_protocol = protocol;
    };
    // 设置请求头，头部以键值对的形式存储在unordered_map中
    void AddHeader(const std::string& key, const std::string& value);
    // 设置请求体，体以字符串的形式存储
    void SetBody(const std::string_view& body) {
        m_body = body;
    };
    void SetUrl(const std::string_view& url) {
        m_url = url;
    };

    void ParseRequestLine(const char c);
    void ParseHeader(const char c);
    void ParseBody(const char c);

    void ParsePath();
    void ParsePost();
    void ParseFromUrlencoded();
};