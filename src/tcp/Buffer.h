#pragma once
#include <string>

class Buffer
{
private:
    /* data */
    std::string buf;

public:
    Buffer(/* args */);
    ~Buffer();
    void append(const char *_buf, size_t _size);
    size_t size();
    const char *c_str();
    const std::string& str() const {
        return buf;
    }
    void Clear();
    void Getline();
    void SetBuf(const char *);
    void Erase(size_t index, size_t len);
};
