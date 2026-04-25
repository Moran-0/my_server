#include "Buffer.h"
#include <iostream>

Buffer::Buffer(/* args */)
{
}

Buffer::~Buffer()
{
}

void Buffer::append(const char *_buf, size_t _size)
{
    for (size_t i = 0; i < _size; ++i)
    {
        if (_buf[i] == '\0')
        {
            break;
        }
        buf.push_back(_buf[i]);
        // buf.append(_buf);
    }
}

size_t Buffer::size()
{
    return buf.size();
}

const char *Buffer::c_str()
{
    return buf.c_str();
}

void Buffer::Clear()
{
    buf.clear();
}
/// @brief 从标准输入中读取一行进入缓冲
void Buffer::Getline()
{
    Clear();
    std::getline(std::cin, buf);
}
void Buffer::SetBuf(const char *_buf)
{
    buf.clear();
    buf.append(_buf);
}
