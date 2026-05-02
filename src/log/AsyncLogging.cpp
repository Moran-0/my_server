#include "AsyncLogging.h"
#include "LogFile.h"

AsyncLogging::AsyncLogging(const char* logFileName) : m_running(false), m_logFileName(logFileName), m_latch(1) {
    m_currentBuffer = std::make_unique<FileBuffer>();
    m_nextBuffer = std::make_unique<FileBuffer>();
}

AsyncLogging::~AsyncLogging() {
    if (m_running.load(std::memory_order_acquire)) {
        Stop();
    }
}

void AsyncLogging::Stop() {
    if (!m_running.exchange(false, std::memory_order_acq_rel)) {
        return;
    }
    m_cv.notify_all(); // 唤醒所有等待的线程
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void AsyncLogging::Start() {
    bool expected = false;
    if (!m_running.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return;
    }
    m_thread = std::thread([this]() { this->ThreadFunc(); });
    m_latch.wait(); // 等待线程启动完成
}

void AsyncLogging::Append(const char* data, int len) {
    if (len <= 0) {
        return;
    }
    bool needNotify = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_currentBuffer->Available() >= len) {
            m_currentBuffer->Append(data, len);
        } else {
            m_buffers.push_back(std::move(m_currentBuffer));
            if (m_nextBuffer) {
                m_currentBuffer = std::move(m_nextBuffer);
                m_nextBuffer = nullptr;
            } else {
                m_currentBuffer = std::make_unique<FileBuffer>();
            }
            m_currentBuffer->Append(data, len);
            needNotify = true;
        }
    }
    if (needNotify) {
        m_cv.notify_one(); // 唤醒后端线程
    }
}

void AsyncLogging::RequestFlush() {
    m_flushRequested.store(true, std::memory_order_release);
    m_cv.notify_one();
}

void AsyncLogging::ThreadFunc() {
    m_latch.notify(); // 通知主线程线程已启动
    std::unique_ptr<FileBuffer> newCurrentBuffer = std::make_unique<FileBuffer>();
    std::unique_ptr<FileBuffer> newNextBuffer = std::make_unique<FileBuffer>();
    std::unique_ptr<LogFile> logFile = std::make_unique<LogFile>(m_logFileName);

    newCurrentBuffer->Bzero();
    newNextBuffer->Bzero();
    std::vector<std::unique_ptr<FileBuffer>> buffersToWrite;

    while (true) {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait_for(lock, std::chrono::milliseconds(static_cast<int>(BufferWriteTimeOut * 1000)), [this]() {
                return !m_running.load(std::memory_order_relaxed) || !m_buffers.empty() || m_flushRequested.load(std::memory_order_relaxed);
            }); // 等待缓冲区有数据可写、超时、收到flush请求或停止请求

            if (!m_currentBuffer) {
                m_currentBuffer = std::move(newCurrentBuffer);
            }
            if (m_currentBuffer && m_currentBuffer->Len() > 0) {
                m_buffers.push_back(std::move(m_currentBuffer)); // 将当前缓冲区加入待写缓冲区队列
            }

            if (m_buffers.empty() && !m_running.load(std::memory_order_relaxed)) {
                break;
            }

            buffersToWrite.swap(m_buffers);
            if (!newCurrentBuffer) {
                newCurrentBuffer = std::make_unique<FileBuffer>();
            }
            m_currentBuffer = std::move(newCurrentBuffer);
            if (!m_nextBuffer) {
                m_nextBuffer = std::move(newNextBuffer);
            }
        }

        for (const auto& buffer : buffersToWrite) {
            logFile->Write(buffer->Data(), buffer->Len()); // 将缓冲区数据写入日志文件
        }
        if (m_flushRequested.exchange(false, std::memory_order_acq_rel)) {
            logFile->Flush(); // 日志文件写入磁盘
        }
        if (logFile->WrittenBytes() > FileMaxSize) {
            logFile = std::make_unique<LogFile>(m_logFileName); // 日志文件达到最大大小，创建新文件
        }
        // 重复使用缓冲区
        if (buffersToWrite.size() > 2) {
            buffersToWrite.resize(2);
        }
        if (!newCurrentBuffer && !buffersToWrite.empty()) {
            newCurrentBuffer = std::move(buffersToWrite.back());
            buffersToWrite.pop_back();
            newCurrentBuffer->Reset();
        }
        if (!newNextBuffer && !buffersToWrite.empty()) {
            newNextBuffer = std::move(buffersToWrite.back());
            buffersToWrite.pop_back();
            newNextBuffer->Reset();
        }
        buffersToWrite.clear();
    }
    logFile->Flush();
}
