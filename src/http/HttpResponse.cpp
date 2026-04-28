#include "HttpResponse.h"
#include <string>

HttpResponse::HttpResponse(bool close_connection) : m_statusCode(HttpStatusCode::KUnkonwn), m_closeConnection(close_connection) {};

void HttpResponse::SetStatusCode(HttpStatusCode status_code) {
    m_statusCode = status_code;
}

void HttpResponse::SetStatusMessage(std::string status_message) {
    m_statusMessage = std::move(status_message);
}

void HttpResponse::SetCloseConnection(bool close_connection) {
    m_closeConnection = close_connection;
}

void HttpResponse::SetContentType(std::string content_type) {
    AddHeader("Content-Type", content_type);
}

void HttpResponse::AddHeader(const std::string& key, const std::string& value) {
    m_headers[key] = value;
}

void HttpResponse::SetBody(std::string body) {
    m_body = std::move(body);
}

bool HttpResponse::IsCloseConnection() const {
    return m_closeConnection;
}

std::string HttpResponse::message() {
    std::string message;
    message += ("HTTP/1.1 " + std::to_string(m_statusCode) + " " + m_statusMessage + "\r\n");
    if (m_closeConnection) {
        message += ("Connection: close\r\n");
    } else {
        message += ("Content-Length: " + std::to_string(m_body.size()) + "\r\n");
        message += ("Connection: Keep-Alive\r\n");
    }

    for (const auto& header : m_headers) {
        message += (header.first + ": " + header.second + "\r\n");
    }

    message += "\r\n";
    message += m_body;

    return message;
}
