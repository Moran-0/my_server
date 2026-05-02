#pragma once
#include "Common.h"

#include <string>
#include <vector>

class Buffer : NoCopy, NoMove {
  private:
    /* data */
    // std::string buf;
    std::vector<char> m_buffer;
    int m_readIndex;
    int m_writeIndex;

  public:
    Buffer();
    ~Buffer() = default;
    char* Begin();
    const char* Begin() const;

    char* BeginRead();
    const char* BeginRead() const;

    char* BeginWrite();
    const char* BeginWrite() const;

    void Append(const char* message);
    void Append(const char* message, int size);
    void Append(const std::string& message);

    int ReadableBytes() const;
    int WritableBytes() const;
    int PrependableBytes() const;

    // 查看数据，但是不更新readIndex位置
    char* Peek();
    const char* Peek() const;
    std::string PeekString(int len) const;
    std::string PeekString() const;

    // 取数据，取出后更新readIndex位置
    void Retrieve(int len);
    std::string RetrieveString(int len);
    void RetrieveAll();
    std::string RetrieveAllString();

    void RetrieveUntil(const char* end);
    std::string RetrieveStringUntil(const char* end);

    void EnsureWritableBytes(int len); // 查看空间
};
