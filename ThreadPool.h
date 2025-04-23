#ifndef THREAD_POOL_H 
#define THREAD_POOL_H
#include <condition_variable>
#include <vector>
#include <thread>
#include <type_traits>
#include <queue>
#include <functional>
#include <mutex>
#include <algorithm>
#include <memory>
#include <future>


class ThreadPool
{
public:
    explicit ThreadPool(uint32_t size);

    ~ThreadPool();

    template<class F, typename ... Args>
    decltype(auto) Enqueue(F&&, Args&& ... args);
private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::condition_variable cond_;
    std::mutex mutex_;
    bool stop_ = false;
};

ThreadPool::ThreadPool(uint32_t size)
{
    for (int i = 0; i < size; i++) {
        workers.emplace_back([this](){
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(mutex_);
                    cond_.wait(lock, [this] { return stop_ || !this->tasks.empty(); });

                    if (stop_ && this->tasks.empty()){
                        return;
                    }
                    task = std::move(this->tasks.front());
                    this->tasks.pop();
                }
                task();
            }
        });
    }
}

inline ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        stop_ = true;
    }
    cond_.notify_all();
    for_each(workers.begin(), workers.end(), std::mem_fn(&std::thread::join));
}


template<class F, typename ...Args>
decltype(auto) ThreadPool::Enqueue(F&& f, Args && ...args)
{
    using return_type = std::invoke_result_t<F, Args...>;

    auto task = std::make_shared< std::packaged_task<return_type()> >(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    std::future<return_type> res = task->get_future();

    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (stop_)
            throw std::runtime_error("enqueue on stopped ThreadPool");

        tasks.emplace([task]() { (*task)(); });
    }
    cond_.notify_one();
    return res;
}


#endif // THREAD_POOL_H
