#include "LogStream.h"
#include <algorithm>
#include <cstdio>
#include <type_traits>

namespace {

constexpr char digits[] = "9876543210123456789";
constexpr const char* zero = digits + 9;
constexpr int maxNumericSize = 32;

template <typename T>
void appendFormatted(LogStream::LogBuffer& buffer, const char* format, T value) {
    if (buffer.Available() >= maxNumericSize) {
        const int len = snprintf(buffer.Current(), maxNumericSize, format, value);
        if (len > 0 && len < maxNumericSize) {
            buffer.Add(static_cast<buffer_size>(len));
        }
    }
}

} // namespace

// From muduo
template <typename T>
buffer_size convert(char buf[], T value) {
    using UnsignedT = std::make_unsigned_t<T>;
    UnsignedT i = 0;
    bool negative = false;

    if constexpr (std::is_signed_v<T>) {
        if (value < 0) {
            negative = true;
            i = static_cast<UnsignedT>(-(value + 1)) + 1;
        } else {
            i = static_cast<UnsignedT>(value);
        }
    } else {
        i = value;
    }

    char* p = buf;

    do {
        int lsd = static_cast<int>(i % 10);
        i /= 10;
        *p++ = zero[lsd];
    } while (i != 0);

    if (negative) {
        *p++ = '-';
    }
    *p = '\0';
    std::reverse(buf, p);

    return p - buf;
}

template <typename T>
void LogStream::FormatInteger(T val) {
    // Reserve enough room so integer formatting is always complete.
    if (m_logBuffer.Available() >= m_maxNumericSize) {
        buffer_size len = convert(m_logBuffer.Current(), val);
        m_logBuffer.Add(len);
    }
}

LogStream& LogStream::operator<<(short v) {
    *this << static_cast<int>(v);
    return *this;
}

LogStream& LogStream::operator<<(unsigned short v) {
    *this << static_cast<unsigned int>(v);
    return *this;
}

LogStream& LogStream::operator<<(int v) {
    FormatInteger(v);
    return *this;
}

LogStream& LogStream::operator<<(unsigned int v) {
    FormatInteger(v);
    return *this;
}

LogStream& LogStream::operator<<(long v) {
    FormatInteger(v);
    return *this;
}

LogStream& LogStream::operator<<(unsigned long v) {
    FormatInteger(v);
    return *this;
}

LogStream& LogStream::operator<<(long long v) {
    FormatInteger(v);
    return *this;
}

LogStream& LogStream::operator<<(unsigned long long v) {
    FormatInteger(v);
    return *this;
}

LogStream& LogStream::operator<<(double v) {
    appendFormatted(m_logBuffer, "%.12g", v);
    return *this;
}

LogStream& LogStream::operator<<(long double v) {
    appendFormatted(m_logBuffer, "%.12Lg", v);
    return *this;
}

LogStream& LogStream::operator<<(const void* p) {
    appendFormatted(m_logBuffer, "%p", p);
    return *this;
}
