#include "Buffer.h"
#include <iostream>
#include <cstring>
#include <stdexcept>

namespace {
constexpr int kInitialSize = 1024;
constexpr int kMaxSize = 1024 * 1024;
constexpr int kPrePendIndex = 8;
} // namespace

Buffer::Buffer() : m_buffer(kInitialSize), m_readIndex(kPrePendIndex), m_writeIndex(kPrePendIndex) {}

char* Buffer::Begin() {
    return &*m_buffer.begin();
}

const char* Buffer::Begin() const {
    return &*m_buffer.begin();
}

char* Buffer::BeginRead() {
    return Begin() + m_readIndex;
}

const char* Buffer::BeginRead() const {
    return Begin() + m_readIndex;
}

char* Buffer::BeginWrite() {
    return Begin() + m_writeIndex;
}

const char* Buffer::BeginWrite() const {
    return Begin() + m_writeIndex;
}

void Buffer::Append(const char* message) {
    Append(message, static_cast<int>(strlen(message)));
}

void Buffer::Append(const char* message, int size) {
    EnsureWritableBytes(size);
    std::copy(message, message + size, BeginWrite());
    m_writeIndex += size;
}

void Buffer::Append(const std::string& message) {
    Append(message.c_str(), static_cast<int>(message.size()));
}

int Buffer::ReadableBytes() const {
    return m_writeIndex - m_readIndex;
}

int Buffer::WritableBytes() const {
    return static_cast<int>(m_buffer.size()) - m_writeIndex;
}

int Buffer::PrependableBytes() const {
    return m_readIndex;
}

char* Buffer::Peek() {
    return BeginRead();
}

const char* Buffer::Peek() const {
    return BeginRead();
}

std::string Buffer::PeekString(int len) const {
    if (len > ReadableBytes()) {
        return {};
    }
    return {BeginRead(), BeginRead() + len};
}

std::string Buffer::PeekString() const {
    return {BeginRead(), BeginWrite()};
}

void Buffer::Retrieve(int len) {
    if (len > ReadableBytes()) {
        throw std::out_of_range("Buffer::Retrieve: len > ReadableBytes");
    }
    if (len < ReadableBytes()) {
        m_readIndex += len;
    } else {
        RetrieveAll();
    }
}

std::string Buffer::RetrieveString(int len) {
    if (len > ReadableBytes()) {
        throw std::out_of_range("Buffer::Retrieve: len > ReadableBytes");
    }
    std::string result{std::move(PeekString(len))};
    Retrieve(len);
    return result;
}

void Buffer::RetrieveAll() {
    m_readIndex = kPrePendIndex;
    m_writeIndex = kPrePendIndex;
}

std::string Buffer::RetrieveAllString() {
    std::string result(std::move(PeekString()));
    RetrieveAll();
    return result;
}

void Buffer::RetrieveUntil(const char* end) {
    if (end > BeginWrite()) {
        throw std::out_of_range("Buffer::RetrieveUntil: end > BeginWrite");
    }
    m_readIndex += static_cast<int>(end - BeginRead());
}

std::string Buffer::RetrieveStringUntil(const char* end) {
    if (end > BeginWrite()) {
        throw std::out_of_range("Buffer::RetrieveUntil: end > BeginWrite");
    }
    std::string result{std::move(PeekString(static_cast<int>(end - BeginRead())))};
    RetrieveUntil(end);
    return result;
}

void Buffer::EnsureWritableBytes(int len) {
    if (WritableBytes() >= len) {
        return;
    }

    const int readable = ReadableBytes();
    if (PrependableBytes() + WritableBytes() - kPrePendIndex >= len) {
        std::memmove(Begin() + kPrePendIndex, BeginRead(), readable);
        m_readIndex = kPrePendIndex;
        m_writeIndex = m_readIndex + readable;
        return;
    }

    const int newSize = m_writeIndex + len;
    if (newSize > kMaxSize) {
        throw std::length_error("Buffer::EnsureWritableBytes: buffer size exceeds limit");
    }
    m_buffer.resize(newSize);
}
