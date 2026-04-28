#pragma once
#include <string>
#include <unordered_map>
class HttpResponse {
  public:
    enum HttpStatusCode {
        KUnkonwn = 1,
        K100Continue = 100,
        K200K = 200,
        K400BadRequest = 400,
        K403Forbidden = 403,
        K404NotFound = 404,
        K500internalServerError = 500
    };
    HttpResponse(bool close_connection);
    ~HttpResponse() = default;

    void SetStatusCode(HttpStatusCode status_code); // 设置回应码
    void SetStatusMessage(std::string status_message);
    void SetCloseConnection(bool close_connection);

    void SetContentType(std::string content_type);
    void AddHeader(const std::string& key, const std::string& value); // 设置回应头

    void SetBody(std::string body);

    std::string message(); // 将信息加入到buffer中。

    bool IsCloseConnection() const;

  private:
    std::unordered_map<std::string, std::string> m_headers;

    HttpStatusCode m_statusCode;
    std::string m_statusMessage;
    std::string m_body;
    bool m_closeConnection;
};