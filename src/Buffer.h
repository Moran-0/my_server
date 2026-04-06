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
    void clear();
    void getline();
    void setBuf(const char *);
};
