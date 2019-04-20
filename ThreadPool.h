#define THREADPOOL_MAX_NUM 16

#include <iostream>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <vector>
#include <mutex>
#include <functional>
#include <condition_variable>
#include <queue>
#include <future>
#include <memory>
#include <atomic>
#include <thread>
#include <chrono>

class ThreadPool
{
  public:
    // 线程组
    std::vector<std::thread> pool;
    // 生产者、消费者的互斥锁
    std::mutex taskQueueMutex;
    // 条件变量
    std::condition_variable condition;
    // 任务队列
    std::queue<std::function<void()>> taskQueue;
    // 线程池运行状态
    std::atomic<bool> is_run;

    ThreadPool(size_t);

    void createThread(size_t);

    template <class F, class... Args>
    auto addTask(F &&f, Args... args) -> std::future<decltype(f(args...))>;

    ~ThreadPool();
};

// 开启线程数量，默认4个
ThreadPool::ThreadPool(size_t thread_num = 4)
{
    is_run = true;
    createThread(thread_num);
}

// 建立线程
void ThreadPool::createThread(size_t thread_num)
{
    for (; (pool.size() < THREADPOOL_MAX_NUM) && (thread_num > 0); --thread_num)
    {
        // 开启线程
        pool.emplace_back(
            [this] {
                // 循环取出任务
                while (is_run)
                {
                    // 单线程所需要执行的任务
                    std::function<void()> task;
                    //std::cout << "开始循环取出任务" << std::endl;
                    // 取出任务
                    {
                        // 生产者和消费者可能会冲突，所以此处操作任务池需要加锁
                        std::unique_lock<std::mutex> lock(taskQueueMutex);
                        /**
                         * 设置线程挂起条件：
                         * 1. 如果没有任务需要执行，则挂起线程，等待任务进入后唤醒;
                         * 2. 如果还有任务需要执行，则继续取出任务执行，不挂起线程;
                        */
                        condition.wait(
                            lock,
                            // 设置条件，任务队列为空，线程池在运行
                            [this] { return !taskQueue.empty() || !is_run; });
                        // 如果没有任务了，并且线程池关闭，则退出
                        if (taskQueue.empty() && !is_run)
                        {
                            //std::cout << "没有任务了" << std::endl;
                            return;
                        }
                        // 取出顶部任务
                        task = std::move(taskQueue.front());
                        //std::cout << "取出一个任务" << std::endl;
                        // 取出后删除任务
                        taskQueue.pop();
                    }
                    // 执行任务
                    task();
                    //std::cout << "执行一个任务" << std::endl;
                }
            });
        //std::cout << "开启一个线程" << std::endl;
    }
}

// 添加任务
template <class F, class... Args>
auto ThreadPool::addTask(F &&f, Args... args) -> std::future<decltype(f(args...))>
{
    // 定义任务返回值类型
    using returnType = decltype(f(args...));
    // 包装任务
    auto task = std::make_shared<std::packaged_task<returnType()>>(
        // 绑定方法
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    // 绑定任务共享返回对象
    std::future<returnType> result = task->get_future();
    {
        // 加锁
        std::lock_guard<std::mutex> lock(taskQueueMutex);
        // 添加任务进入队列
        taskQueue.emplace(
            [task] {
                (*task)();
            });
        //std::cout << "添加一个任务" << std::endl;
    }
    // 唤醒线程执行任务
    condition.notify_one();

    return result;
}

ThreadPool::~ThreadPool()
{
    // 关闭运行状态
    is_run = false;
    // 空闲所有线程
    condition.notify_all();
    // 等待任务执行完，关闭所有线程
    for (std::thread &thread : pool)
    {
        if (thread.joinable())
            thread.join();
    }
}