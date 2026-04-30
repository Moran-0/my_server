#include "Timer.h"
#include <chrono>

using std::chrono::duration_cast;
using std::chrono::steady_clock;

Timer::Timer(int timeout, std::function<void()> cb) : m_deleted(false), m_taskCallback(std::move(cb)) {
    auto now = steady_clock::now();
    m_expiredTime = duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() + timeout; // 以毫秒为单位计算超时时间点
}

void Timer::Run() {
    auto cb = std::move(m_taskCallback);
    m_taskCallback = nullptr;
    if (cb) {
        cb();
    }
}

void Timer::Update(int timeout) {
    auto now = steady_clock::now();
    m_expiredTime = duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() + timeout; // 以毫秒为单位计算新的超时时间点
}

bool Timer::IsValid() {
    auto curTime = duration_cast<std::chrono::milliseconds>(steady_clock::now().time_since_epoch()).count();
    if (curTime < m_expiredTime) {
        return true;
    }
    this->SetDeleted();
    return false;
}

void Timer::ClearCallback() {
    m_taskCallback = nullptr;
    SetDeleted();
}

void Timer::SetDeleted() {
    m_deleted = true;
}

void TimerManager::HandleExpiredEvent() {
    while (!timerQueue.empty()) {
        auto timer = timerQueue.top();
        if (timer->IsDeleted() || !timer->IsValid()) {
            timerQueue.pop();
            timer->Run();
        } else {
            break; // 队首的Timer未过期，后续的Timer也未过期
        }
    }
}

int TimerManager::GetNextTimeOut() {
    // 没有定时任务时返回-1,让epoll_wait无限等待
    if (timerQueue.empty()) {
        return -1;
    }
    auto timer = timerQueue.top();
    auto now = std::chrono::steady_clock::now();
    int remain = timer->GetExpTime() - duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    return remain > 0 ? remain : 0;
}
