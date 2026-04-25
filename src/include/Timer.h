#pragma once
#include <unistd.h>
#include <deque>
#include <memory>
#include <queue>

using std::shared_ptr;

class HttpConnect;

class TimerNode {
  public:
    TimerNode(shared_ptr<HttpConnect> requestData, int timeout); // timeout单位为毫秒
    ~TimerNode();
    TimerNode(TimerNode& tn_);
    void update(int timeout);
    bool isValid();
    void clearRequest();
    void setDeleted();
    [[nodiscard]] bool isDeleted() const {
        return m_deleted;
    }
    [[nodiscard]] size_t getExpTime() const {
        return m_expiredTime;
    }

  private:
    bool m_deleted;
    size_t m_expiredTime;
    shared_ptr<HttpConnect> m_httpData;
};