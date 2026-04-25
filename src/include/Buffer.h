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
    void Clear();
    void Getline();
    void SetBuf(const char *);
};
