#pragma once
#include <vector>
#include <thread>
#include <queue>
#include <functional>
#include <condition_variable>
#include <mutex>
#include <future>

/**暂时被EventLoopThreadPool替代 */

constexpr int MAX_POOL_SIZE = 256;
constexpr int MAX_TASK_SIZE = 4096;
using std::vector;
class ThreadPool {
  private:
    /* data */
    vector<std::thread> m_threads;
    std::queue<std::function<void()>> m_tasks;
    std::mutex m_mtx;
    std::condition_variable m_cv;
    bool m_stop;

  public:
    explicit ThreadPool(int size = std::thread::hardware_concurrency());
    ~ThreadPool();
    // void addTask(std::function<void()> task);
    template <class F, class... Args> auto addTask(F&& func, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type>;
};

template <class F, class... Args> auto ThreadPool::addTask(F&& func, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type> {
    using return_type = typename std::result_of<F(Args...)>::type;
    auto task = std::make_shared<std::packaged_task<return_type()>>(std::bind(std::forward<F>(func), std::forward<Args>(args)...));
    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(m_mtx);
        if (m_stop)
            throw std::runtime_error("线程池已停止！");
        m_tasks.emplace([task]() { (*task)(); });
    }
    m_cv.notify_one();
    return res;
}
