#include "Logging.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <limits>

namespace {

std::atomic<Logger::LogLevel> g_logLevel {Logger::LogLevel::INFO};
std::atomic<Logger::OutputFunc> g_output {nullptr};
std::atomic<Logger::FlushFunc> g_flush {nullptr};

void defaultOutput(const char* data, int len) {
    fwrite(data, 1, static_cast<size_t>(len), stdout);
}

void defaultFlush() {
    fflush(stdout);
}

const char* LogLevelName(Logger::LogLevel level) {
    switch (level) {
    case Logger::LogLevel::DEBUG:
        return "DEBUG";
    case Logger::LogLevel::INFO:
        return "INFO";
    case Logger::LogLevel::WARN:
        return "WARN";
    case Logger::LogLevel::ERROR:
        return "ERROR";
    case Logger::LogLevel::FATAL:
        return "FATAL";
    }
    return "UNKNOWN";
}

const char* SourceFileName(const char* path) {
    if (path == nullptr) {
        return "unknown";
    }

    const char* file = path;
    for (const char* p = path; *p != '\0'; ++p) {
        if (*p == '/' || *p == '\\') {
            file = p + 1;
        }
    }
    return file;
}

} // namespace

// ===================== Logger 静态方法 =====================

Logger::LogLevel Logger::getLogLevel() {
    return g_logLevel.load(std::memory_order_relaxed);
}

void Logger::setLogLevel(LogLevel level) {
    g_logLevel.store(level, std::memory_order_relaxed);
}

bool Logger::IsLevelEnabled(LogLevel level) {
    return getLogLevel() <= level;
}

void Logger::setOutput(OutputFunc func) {
    g_output.store(func, std::memory_order_release);
}

void Logger::setFlush(FlushFunc func) {
    g_flush.store(func, std::memory_order_release);
}

// ===================== Logger =====================

Logger::Logger(const char* fileName, LogLevel level, int line) : m_impl(fileName, level, line) {
    m_impl.Stream() << LogLevelName(level) << ' ';
}

Logger::~Logger() {
    m_impl.Finish();

    const auto& buf = m_impl.Stream().Buffer();
    auto* output = g_output.load(std::memory_order_acquire);
    if (output == nullptr) {
        output = defaultOutput;
    }

    const auto len = static_cast<int>(std::min(buf.Len(), static_cast<buffer_size>(std::numeric_limits<int>::max())));
    output(buf.Data(), len);

    if (m_impl.GetLevel() >= LogLevel::ERROR) {
        auto* flush = g_flush.load(std::memory_order_acquire);
        if (flush == nullptr) {
            flush = defaultFlush;
        }
        flush();
    }

    if (m_impl.GetLevel() == LogLevel::FATAL) {
        abort();
    }
}

LogStream& Logger::Stream() {
    return m_impl.Stream();
}

const char* Logger::FormattedTime() {
    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    if (localtime_r(&tt, &tm) == nullptr) {
        return "00000000 00:00:00 ";
    }
    thread_local static std::array<char, 32> buf{};
    auto len = std::strftime(buf.data(), buf.size(), "%Y%m%d %H:%M:%S ", &tm);
    if (len == 0) {
        return "00000000 00:00:00 ";
    }
    return buf.data();
}

// ===================== Logger::Impl =====================

Logger::Impl::Impl(const char* sourceFile, LogLevel level, int line) : m_sourceFile(SourceFileName(sourceFile)), m_line(line), m_level(level) {
    m_stream << FormattedTime();
}

void Logger::Impl::Finish() {
    m_stream << " - " << m_sourceFile << ':' << m_line << '\n';
}

LogStream& Logger::Impl::Stream() {
    return m_stream;
}

const char* Logger::Impl::GetLogLevelName() const {
    return LogLevelName(m_level);
}
