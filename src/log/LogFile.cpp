#include "LogFile.h"
#include "Logging.h"
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>
#include <chrono>

static constexpr int FlustInterval = 3;

namespace {
FILE* OpenLogFile(const std::filesystem::path& path) {
    if (path.empty()) {
        return nullptr;
    }

    const auto parent = path.parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
    }
    return ::fopen(path.string().c_str(), "a+");
}

std::string LogFileTimeSuffix() {
    std::string suffix = Logger::FormattedTime();
    for (auto& ch : suffix) {
        if (ch == ' ' || ch == ':') {
            ch = '_';
        }
    }
    return suffix;
}
} // namespace

LogFile::LogFile(const char* filename) : m_file(nullptr), m_writtenBytes(0), m_lastWriteTime(0), m_lastFlushTime(0) {
    if (filename != nullptr && filename[0] != '\0') {
        m_file = OpenLogFile(filename);
    }

    if (m_file == nullptr) {
        const auto defaultName = std::string("LogFile_") + LogFileTimeSuffix() + ".log";
        std::error_code ec;
        const auto tempPath = std::filesystem::temp_directory_path(ec) / defaultName;
        for (const auto& path : {std::filesystem::path("../LogFiles") / defaultName, std::filesystem::path("LogFiles") / defaultName,
                                 std::filesystem::path(defaultName), tempPath}) {
            m_file = OpenLogFile(path);
            if (m_file != nullptr) {
                break;
            }
        }
    }

    if (m_file == nullptr) {
        m_file = ::tmpfile();
    }
}

LogFile::~LogFile() {
    Flush();
    if (m_file) {
        ::fclose(m_file);
    }
}

void LogFile::Write(const char* log, int len) {
    if (m_file == nullptr || len <= 0) {
        return;
    }
    int pos = 0;
    while (pos < len) {
        pos += static_cast<int>(::fwrite_unlocked(log + pos, sizeof(char), len - pos, m_file));
    }
    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    m_lastWriteTime = tt;
    m_writtenBytes += len;
    if (m_lastWriteTime - m_lastFlushTime > FlustInterval) {
        Flush();
        m_lastFlushTime = tt;
    }
}

void LogFile::Flush() {
    if (m_file) {
        ::fflush(m_file);
    }
}
