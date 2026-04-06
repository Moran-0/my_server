#pragma once
#include <vector>
#include <thread>
#include <queue>
#include <functional>
#include <condition_variable>
#include <mutex>
#include <future>

using std::vector;
class ThreadPool
{
private:
    /* data */
    vector<std::thread> threads;
    std::queue<std::function<void()>> tasks;
    std::mutex mtx;
    std::condition_variable cv;
    bool stop;

public:
    ThreadPool(int size = std::thread::hardware_concurrency());
    ~ThreadPool();
    // void addTask(std::function<void()> task);
    template <class F, class... Args>
    auto addTask(F &&func, Args &&...args) -> std::future<typename std::result_of<F(Args...)>::type>;
};

template <class F, class... Args>
auto ThreadPool::addTask(F &&func, Args &&...args) -> std::future<typename std::result_of<F(Args...)>::type>
{
    using return_type = typename std::result_of<F(Args...)>::type;
    auto task = std::make_shared<std::packaged_task<return_type()>>(std::bind(std::forward<F>(func), std::forward<Args>(args)...));
    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(mtx);
        if (stop)
            throw std::runtime_error("线程池已停止！");
        tasks.emplace([task]()
                      { (*task)(); });
    }
    cv.notify_one();
    return res;
}
