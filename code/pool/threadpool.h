#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <thread>
#include <assert.h>

class ThreadPool
{
public:
	ThreadPool() = default;
	ThreadPool(ThreadPool &&) = default; // 移动构造函数的声明
	// 尽量用make_shared代替new，如果通过new再传递给shared_ptr，内存是不连续的，会造成内存碎片化
	/*这个构造函数创建了一个线程池，并启动多个线程来处理任务队列中的任务。每个线程在无限循环中检查任务队列，执行任务，并在没有任务时等待新任务的到来。 */
	explicit ThreadPool(int threadCount = 8) : pool_(std::make_shared<Pool>()) {  // make_shared:传递右值，功能是在动态内存中分配一个对象并初始化它，返回指向此对象的shared_ptr
		assert(threadCount > 0); // 确保线程数大于0
		// 创建threadCount个线程，每个线程都会执行下面的lambda表达式
		for(int i = 0; i < threadCount; i++) {
			std::thread([this]() {
				std::unique_lock<std::mutex> locker(pool_->mtx_);
				while(true) {
					//如果任务队列不为空，从队列中取出任务并解锁互斥锁，然后执行任务，任务执行完后重新上锁。
					if(!pool_->tasks.empty()) {
						/*左值（lvalue）
						定义：左值是指可以标识一个对象的表达式。左值表达式的结果是一个对象的内存地址，可以对其进行取地址操作。
						特性：
						1.可以出现在赋值操作符的左侧。
						2.可以取地址（使用 & 操作符）。
						3.通常是变量、数组元素、解引用指针等。

						右值（rvalue）
						定义：右值是指不具名的临时对象或字面值。右值表达式的结果是一个值，而不是一个对象的内存地址。
						特性：
						1.不能出现在赋值操作符的左侧。
						2.不能取地址。
						3.通常是字面值、临时对象、表达式的结果等。*/
						auto task = std::move(pool_->tasks.front()); // 左值变右值，资产转移
						pool_->tasks.pop();
						locker.unlock();  // 因为已经把任务取出来了，所以可以提前解锁了
						task();
						locker.lock(); // 马上又要取任务了，上锁
					} else if(pool_->isClosed) {
						break;
					} else {
						pool_->cond_.wait(locker); // 等待，如果任务来了就notify的
					}
				}			
			}).detach(); // detach() 将线程与主线程分离，使其在后台运行。
		}

	}

	/*这个析构函数确保了当 ThreadPool 对象被销毁时，所有正在等待任务的线程都能被唤醒并正确退出，
	从而避免线程无限期地等待新任务。通过设置 isClosed 标志和唤醒所有线程，析构函数实现了线程池的安全关闭。*/
	~ThreadPool() {
		if(pool_) {
			std::unique_lock<std::mutex> locker(pool_->mtx_);
			pool_->isClosed = true;
		}
		pool_->cond_.notify_all(); // 唤醒所有的线程
	}

	// 将一个任务添加到任务队列中，并通知一个等待的线程有新任务到来。
	template<typename T>
	void AddTask(T&& task) { // 方法名为 AddTask，接受一个右值引用参数 task。
		std::unique_lock<std::mutex> locker(pool_->mtx_);
		/*使用 std::forward 将任务完美转发到任务队列中。
		emplace 方法会在队列中直接构造任务对象，避免不必要的拷贝。*/
		pool_->tasks.emplace(std::forward<T>(task));
		pool_->cond_.notify_one();  // 通知一个等待的线程有新任务到来
	}


private:
	// 用一个结构体封装起来，方便调用
	struct Pool {
		std::mutex mtx_;  // 互斥锁,保护任务队列
		std::condition_variable cond_;  // 条件变量，用于线程间通信
		bool isClosed;  // 标志位，线程池是否关闭
		/*存储待执行的任务。
		任务类型为 std::function<void()>，表示不接受参数且没有返回值的函数。
		可以存储任意可调用对象，如函数指针、lambda 表达式、绑定表达式等。*/
		std::queue<std::function<void()>> tasks; // 任务队列，函数类型为void()的队列
	};
	std::shared_ptr<Pool> pool_; // shared_ptr是一个智能指针，可以自动释放内存
};

#endif