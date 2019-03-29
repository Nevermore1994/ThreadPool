#ifndef THREAD_POOL_H 
#define THREAD_POOL_H
#include <condition_variable>
#include <vector>
#include <Thread>
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
	ThreadPool(size_t size);
	~ThreadPool();
	template<class F, typename ... Args>
	decltype(auto) Enqueue(F&&, Args&& ... args); //通过完美转发进行任务入队列
private:
	std::vector<std::thread> workers; //用来工作的线程
	std::queue<std::function<void()>> tasks; //线程池的任务队列
	std::condition_variable cond_; 
	std::mutex mutex_; 
	bool stop;
};

inline ThreadPool::ThreadPool(size_t size) :stop(false)
{
	for (int i = 0; i < size; i++)
	{
		workers.emplace_back([this]()
		{
			while (true) //死循环等待
			{
				std::function<void()> task;
				{   
					//临界区开始 等待线程池的销毁或者是任务的带来
					std::unique_lock<std::mutex> lock(mutex_);
					cond_.wait(lock, [this] { return this->stop || !this->tasks.empty(); }); //条件变量的wait函数 防止虚假唤醒

					if (this->stop && this->tasks.empty())
					{
						return; //线程池销毁 跳出循环
					}

					task = std::move(this->tasks.front()); //将队列的头部移动给task
					this->tasks.pop(); //记得出队列
					
					//临界区结束 自动释放锁 mutex_
				}
				task(); //执行任务
			}
		});
	}
}

inline ThreadPool::~ThreadPool()
{
	{
		//临界区开始  锁住stop 将其赋值为true 通知线程退出循环
		std::unique_lock<std::mutex> lock(mutex_);
		stop = true;
		//临界区结束 自动释放锁 mutex_
	}
	cond_.notify_all();//通知所有的线程退出循环
	for_each(workers.begin(), workers.end(), std::mem_fn(&std::thread::join)); //等待
}

//原来的函数是用返回值尾序来获取类型 在C++14之后支持decltype(auto)进行类型的推导（不熟悉decltype推导规则可以看看）
template<class F, typename ...Args>
decltype(auto) ThreadPool::Enqueue(F&& f, Args && ...args)
{
	using return_type = typename std::result_of_t<F(Args...)>; //获取返回值类型

	auto task = std::make_shared< std::packaged_task<return_type()> >(
		std::bind(std::forward<F>(f), std::forward<Args>(args)...) //通过bind和完美转发将函数和参数绑定
		);

	std::future<return_type> res = task->get_future();
	
	{
		//临界区开始 锁任务队列
		std::unique_lock<std::mutex> lock(mutex_);
		
		//如果已经将线程池析构,就不允许再入队列
		if (stop)
			throw std::runtime_error("enqueue on stopped ThreadPool");

		tasks.emplace([task]() { (*task)(); });
	}
	cond_.notify_one(); //通知任意一个线程接收任务
	return res;
}

#endif // THREAD_POOL_H
