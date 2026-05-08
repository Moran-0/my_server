#pragma once
#include <string>
#include <vector>
#include <unordered_map>

struct MultipartPart {
    std::string name;
    std::string filename;
    std::string contentType;
    std::string content;
    bool isFile() const {
        return !filename.empty();
    };
};

class MultipartParser {
  public:
    static bool Parse(const std::string& contentType, const std::string& body, std::vector<MultipartPart>& parts, std::string& errorMsg);

  private:
    static bool ExtractBoundary(const std::string& contentType, std::string& boundary);
    static std::unordered_map<std::string, std::string> ParsePartHeaders(const std::string& headers);
    static std::string GetDispositionValue(const std::string& header, const std::string& key);
    static std::string Trim(const std::string& str);
};
