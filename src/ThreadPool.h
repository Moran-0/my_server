#pragma once
#include <vector>
#include <thread>
#include <queue>
#include <functional>
#include <condition_variable>
#include <mutex>

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
    ThreadPool(/* args */);
    ~ThreadPool();
    void addTask(std::function<void()> task);
};
