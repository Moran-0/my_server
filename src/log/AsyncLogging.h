#pragma once
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <thread>

#include "LogStream.h"
#include "Common.h"
constexpr double BufferWriteTimeOut = 3.0; // 每3秒flush一次缓冲区
constexpr buffer_size AsyncLogBufferSize = MID_BUFFER_SIZE;
constexpr int64_t FileMaxSize = 1024 * 1024 * 1024; // 单个文件最大的容量

// 简单是实现c++20的std::latch
class Latch : NoCopy, NoMove {
  private:
    std::mutex m_mtx;
    std::condition_variable m_cv;
    int m_count;

  public:
    explicit Latch(int count) : m_count(count) {}
    void wait() {
        std::unique_lock<std::mutex> lock(m_mtx);
        while (m_count > 0) {
            m_cv.wait(lock);
        }
    }

    void notify() {
        std::unique_lock<std::mutex> lock(m_mtx);
        --m_count;
        if (m_count == 0) {
            m_cv.notify_all();
        }
    }
};

class AsyncLogging {
  public:
    using FileBuffer = FixBuffer<AsyncLogBufferSize>;
    AsyncLogging(const char* logFileName = nullptr);
    ~AsyncLogging();

    void Stop();
    void Start();
    void Append(const char* data, int len);
    void RequestFlush();
    void ThreadFunc();

  private:
    std::atomic<bool> m_running;
    const char* m_logFileName;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    Latch m_latch;
    std::thread m_thread;
    std::atomic<bool> m_flushRequested{false};

    std::unique_ptr<FileBuffer> m_currentBuffer;
    std::unique_ptr<FileBuffer> m_nextBuffer;
    std::vector<std::unique_ptr<FileBuffer>> m_buffers;
};
