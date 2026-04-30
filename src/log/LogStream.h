#pragma once
#include "Common.h"
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>

using buffer_size = std::size_t;
constexpr buffer_size MIN_BUFFER_SIZE = 4096;
constexpr buffer_size MID_BUFFER_SIZE = 4096 * 500;
constexpr buffer_size MAX_BUFFER_SIZE = 4096 * 1000;

template <buffer_size SIZE>
class FixBuffer : public NoCopy {
  public:
    FixBuffer() : m_cur(m_data) {}
    ~FixBuffer() = default;
    void Append(const char* buf, buffer_size len) {
        if (buf == nullptr || len == 0) {
            return;
        }

        const buffer_size writable = std::min(Available(), len);
        if (writable > 0) {
            memcpy(m_cur, buf, writable);
            m_cur += writable;
        }
    }
    const char* Data() const {
        return m_data;
    };
    buffer_size Len() const {
        return static_cast<buffer_size>(m_cur - m_data);
    }

    char* Current() {
        return m_cur;
    };

    buffer_size Available() const {
        return static_cast<buffer_size>(End() - m_cur);
    };

    void Reset() {
        m_cur = m_data;
    };
    void Bzero() {
        memset(m_data, 0, sizeof(m_data));
    }
    void Add(buffer_size len) {
        m_cur += std::min(len, Available());
    }

  private:
    [[nodiscard]] const char* End() const {
        return m_data + sizeof(m_data);
    };
    char m_data[SIZE];
    char* m_cur;
};

class LogStream : public NoCopy {
  public:
    using LogBuffer = FixBuffer<MIN_BUFFER_SIZE>;

    LogStream& operator<<(bool val) {
        m_logBuffer.Append(val ? "T" : "F", 1);
        return *this;
    }

    LogStream& operator<<(short);
    LogStream& operator<<(unsigned short);
    LogStream& operator<<(int);
    LogStream& operator<<(unsigned int);
    LogStream& operator<<(long);
    LogStream& operator<<(unsigned long);
    LogStream& operator<<(long long);
    LogStream& operator<<(unsigned long long);

    LogStream& operator<<(const void*);

    LogStream& operator<<(float v) {
        *this << static_cast<double>(v);
        return *this;
    }
    LogStream& operator<<(double);
    LogStream& operator<<(long double);

    LogStream& operator<<(char v) {
        m_logBuffer.Append(&v, 1);
        return *this;
    }

    LogStream& operator<<(const char* str) {
        if (str) {
            m_logBuffer.Append(str, strlen(str));
        } else {
            m_logBuffer.Append("(null)", 6);
        }
        return *this;
    }

    LogStream& operator<<(const unsigned char* str) {
        return operator<<(reinterpret_cast<const char*>(str));
    }

    LogStream& operator<<(std::string_view v) {
        m_logBuffer.Append(v.data(), v.size());
        return *this;
    }

    LogStream& operator<<(const std::string& v) {
        *this << std::string_view(v);
        return *this;
    }

    void Append(const char* buf, buffer_size len) {
        m_logBuffer.Append(buf, len);
    }
    [[nodiscard]] const LogBuffer& Buffer() const {
        return m_logBuffer;
    }
    void ResetBuffer() {
        m_logBuffer.Reset();
    }

  private:
    LogBuffer m_logBuffer;
    template <typename T>
    void FormatInteger(T);
    static constexpr int m_maxNumericSize = 32;
};
