#pragma once
#include <memory>

class LogFile {
  public:
    LogFile(const char* filename = nullptr);
    ~LogFile();
    void Write(const char* log, int len);
    void Flush();
    [[nodiscard]] int64_t WrittenBytes() const {
        return m_writtenBytes;
    };

  private:
    FILE* m_file;
    int64_t m_writtenBytes;
    time_t m_lastWriteTime;
    time_t m_lastFlushTime;
};