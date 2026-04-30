#include "Logging.h"
#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstdio>
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

    auto* flush = g_flush.load(std::memory_order_acquire);
    if (flush == nullptr) {
        flush = defaultFlush;
    }
    flush();

    if (m_impl.GetLevel() == LogLevel::FATAL) {
        abort();
    }
}

LogStream& Logger::Stream() {
    return m_impl.Stream();
}

// ===================== Logger::Impl =====================

Logger::Impl::Impl(const char* sourceFile, LogLevel level, int line) : m_sourceFile(SourceFileName(sourceFile)), m_line(line), m_level(level) {
    FormattedTime();
}

void Logger::Impl::FormattedTime() {
    time_t now = time(nullptr);
    struct tm tm_time;
    if (localtime_r(&now, &tm_time) == nullptr) {
        m_stream << "00000000 00:00:00 ";
        return;
    }
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%04d%02d%02d %02d:%02d:%02d ", tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday, tm_time.tm_hour,
                       tm_time.tm_min, tm_time.tm_sec);
    if (len > 0) {
        m_stream.Append(buf, static_cast<buffer_size>(len));
    }
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
