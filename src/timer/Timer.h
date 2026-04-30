#pragma once
#include <unistd.h>
#include <deque>
#include <memory>
#include <queue>
#include <functional>

using std::shared_ptr;

class HttpConnect;

class Timer {
  public:
    Timer(int timeout, std::function<void()> cb); // timeout单位为毫秒
    ~Timer() = default;
    void Run();
    void Update(int timeout);
    bool IsValid();
    void ClearCallback();
    void SetDeleted();
    [[nodiscard]] bool IsDeleted() const {
        return m_deleted;
    }
    [[nodiscard]] size_t GetExpTime() const {
        return m_expiredTime;
    }

  private:
    bool m_deleted;
    size_t m_expiredTime;
    std::function<void()> m_taskCallback;
};

class TimerCmp {
  public:
    bool operator()(const shared_ptr<Timer>& a, const shared_ptr<Timer>& b) const {
        return a->GetExpTime() > b->GetExpTime(); // 小顶堆，过期时间较早的Timer优先
    }
};

class TimerManager {
  public:
    TimerManager() = default;
    ~TimerManager() = default;
    void AddTimer(const shared_ptr<Timer>& timer) {
        timerQueue.push(timer);
    }
    [[nodiscard]] std::shared_ptr<Timer> AddTimer(int timeout, std::function<void()> cb) {
        auto timer = std::make_shared<Timer>(timeout, std::move(cb));
        timerQueue.push(timer);
        return timer;
    }
    void HandleExpiredEvent();
    int GetNextTimeOut(); // 获取下一次最近超时时间间隔

  private:
    std::priority_queue<shared_ptr<Timer>, std::deque<shared_ptr<Timer>>, TimerCmp> timerQueue;
};