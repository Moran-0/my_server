#include "MultipartParser.h"
#include <algorithm>
bool MultipartParser::Parse(const std::string& contentType, const std::string& body, std::vector<MultipartPart>& parts, std::string& errorMsg) {
    parts.clear();
    errorMsg.clear();
    std::string boundary;
    if (!ExtractBoundary(contentType, boundary)) {
        errorMsg = "Missing multipart boundary";
        return false;
    }
    // RFC协议自动添加--
    const std::string delimiter = "--" + boundary;
    // const std::string closeDelimiter = delimiter + "--";
    size_t pos = 0;
    while (true) {
        size_t boundaryPos = body.find(delimiter, pos);
        if (boundaryPos == std::string::npos) {
            break;
        }
        boundaryPos += delimiter.size();
        if (body.compare(boundaryPos, 2, "--") == 0) {
            return true; // 到达结尾
        }
        if (body.compare(boundaryPos, 2, "\r\n") != 0) {
            errorMsg = "Invalid multipart boundary line";
            return false;
        }
        size_t partStart = pos + 2; // 跳过 \r\n
        size_t headerEnd = body.find("\r\n\r\n", partStart);
        if (headerEnd == std::string::npos) {
            errorMsg = "Invalid multipart part:missing header terminator";
            return false;
        }
        std::string headerText = body.substr(partStart, headerEnd - partStart);
        auto header = ParsePartHeaders(headerText);
        auto dispositionIt = header.find("content-disposition");
        if (dispositionIt == header.end()) {
            errorMsg = "Invalid multipart part:missing content-disposition header";
            return false;
        }
        MultipartPart part;
        part.name = GetDispositionValue(dispositionIt->second, "name");
        part.filename = GetDispositionValue(dispositionIt->second, "filename");
        if (part.name.empty()) {
            errorMsg = "Invalid multipart part:missing name in content-disposition header";
            return false;
        }
        auto contentType = header.find("content-type");
        if (contentType != header.end()) {
            part.contentType = contentType->second;
        }
        size_t dataStart = headerEnd + 4;
        size_t nextBoundary = body.find("\r\n" + delimiter, dataStart);
        if (nextBoundary == std::string::npos) {
            errorMsg = "missing next multipart boundary";
            return false;
        }
        part.content = body.substr(dataStart, nextBoundary - dataStart);
        parts.push_back(std::move(part));
        pos = nextBoundary + 2;
    }
    errorMsg = "missing closing multipart boundary";
    return false;
}

bool MultipartParser::ExtractBoundary(const std::string& contentType, std::string& boundary) {
    const std::string prefix = "boundary=";
    size_t pos = contentType.find(prefix);
    if (pos == std::string::npos) {
        return false;
    }
    boundary = contentType.substr(pos + prefix.size());
    // ;号分隔
    size_t semicolonPos = boundary.find(';');
    if (semicolonPos != std::string::npos) {
        boundary = boundary.substr(0, semicolonPos);
    }
    // 去掉前后空白字符
    boundary = Trim(boundary);
    // 去除引号
    if (boundary.size() > 2 && boundary.front() == '"' && boundary.back() == '"') {
        boundary = boundary.substr(1, boundary.size() - 2);
    }
    return !boundary.empty();
}

std::unordered_map<std::string, std::string> MultipartParser::ParsePartHeaders(const std::string& headers) {
    std::unordered_map<std::string, std::string> headersMap;
    size_t pos = 0;
    while (pos < headers.size()) {
        size_t lineEnd = headers.find("\r\n", pos);
        if (lineEnd == std::string::npos) {
            lineEnd = headers.size();
        }
        size_t colon = headers.find(':', pos);
        if (colon != std::string::npos && colon < lineEnd) {
            std::string key = headers.substr(pos, colon - pos);
            std::string value = headers.substr(colon + 1, lineEnd - colon - 1);
            // std::tolower() 对负值 char 可能有未定义行为
            std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            headersMap.insert(std::make_pair(key, value));
        }
        pos = lineEnd + 2;
    }
    return headersMap;
}

std::string MultipartParser::GetDispositionValue(const std::string& header, const std::string& key) {
    const std::string marker = key + "=\"";
    size_t pos = header.find(marker);
    if (pos == std::string::npos) {
        return "";
    }
    pos += marker.size();
    size_t end = header.find('"', pos);
    if (end == std::string::npos) {
        return "";
    }
    return header.substr(pos, end - pos);
}

std::string MultipartParser::Trim(const std::string& str) {
    size_t start = 0;
    while (start < str.size() && (str[start] == ' ' || str[start] == '\t' || str[start] == '\r' || str[start] == '\n')) {
        ++start;
    }

    size_t end = str.size();
    while (end > start && (str[end - 1] == ' ' || str[end - 1] == '\t' || str[end - 1] == '\r' || str[end - 1] == '\n')) {
        --end;
    }

    return str.substr(start, end - start);
}
