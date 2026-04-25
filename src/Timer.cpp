#include "Timer.h"
#include <chrono>

using std::chrono::duration_cast;
using std::chrono::steady_clock;

TimerNode::TimerNode(shared_ptr<HttpConnect> requestData, int timeout) : m_deleted(false), m_httpData(std::move(requestData)) {
    auto now = steady_clock::now();
    m_expiredTime = duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() + timeout; // 以毫秒为单位计算超时时间点
}

TimerNode::~TimerNode() {}

void TimerNode::update(int timeout) {}

bool TimerNode::isValid() {
    auto curTime = duration_cast<std::chrono::milliseconds>(steady_clock::now().time_since_epoch()).count();
    if (curTime < m_expiredTime) {
        return true;
    } else {
        this->setDeleted();
        return false;
    }
}

void TimerNode::clearRequest() {}

void TimerNode::setDeleted() {}
