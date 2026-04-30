#pragma once
#include "LogStream.h"
#include "Common.h"

class Logger : public NoCopy {
  public:
    enum class LogLevel { DEBUG, INFO, WARN, ERROR, FATAL };
    Logger(const char* fileName, LogLevel level, int line);
    ~Logger();
    LogStream& Stream();

    static LogLevel getLogLevel();
    static void setLogLevel(LogLevel level);
    static bool IsLevelEnabled(LogLevel level);

    using OutputFunc = void (*)(const char* data, int len);
    using FlushFunc = void (*)();
    static void setOutput(OutputFunc);
    static void setFlush(FlushFunc);

  private:
    class Impl : NoCopy {
      public:
        Impl(const char* sourceFile, LogLevel level, int line);
        void FormattedTime();
        void Finish();
        LogStream& Stream();
        const char* GetLogLevelName() const;
        LogLevel GetLevel() const { return m_level; }

      private:
        const char* m_sourceFile;
        int m_line;
        LogLevel m_level;
        LogStream m_stream;
    };
    Impl m_impl;
};

// 日志宏
#define LOG_LEVEL_INTERNAL(level)                                                                                                        \
    if (!Logger::IsLevelEnabled(level)) {                                                                                                \
    } else                                                                                                                               \
        Logger(__FILE__, level, __LINE__).Stream()
#define LOG_DEBUG LOG_LEVEL_INTERNAL(Logger::LogLevel::DEBUG)
#define LOG_INFO LOG_LEVEL_INTERNAL(Logger::LogLevel::INFO)
#define LOG_WARN LOG_LEVEL_INTERNAL(Logger::LogLevel::WARN)
#define LOG_ERROR LOG_LEVEL_INTERNAL(Logger::LogLevel::ERROR)
#define LOG_FATAL Logger(__FILE__, Logger::LogLevel::FATAL, __LINE__).Stream()
