#ifndef BLOCKQUEUE_H
#define BLOCKQUEUE_H

#include <deque>
#include <condition_variable>
#include <mutex>
#include <sys/time.h>
using namespace std;

template <typename T>
class BlockQueue
{
public:
    /*关键字 explicit 用于构造函数时，它告诉编译器这个构造函数是“明确的”，不能用于隐式类型转换或复制初始化。
    这可以防止一些不期望的隐式转换，提高代码的安全性。
    BlockQueue bq = 1000; 隐式初始化 不合法 复制初始化。它尝试使用赋值的方式来创建对象 bq
    BlockQueue bq(1000); // 显式调用构造函数，这是合法的。直接初始化。它明确地调用了构造函数，并将参数 1000 传递给构造函数。
    */
    explicit BlockQueue(size_t maxsize = 1000);
    ~BlockQueue();
    bool empty();
    bool full();
    void push_back(const T &item);
    void push_front(const T &item);
    bool pop(T &item);              // 弹出的任务放入item
    bool pop(T &item, int timeout); // 等待时间
    void clear();
    T front();
    T back();
    // size_t 是一个无符号整数类型
    size_t capacity();
    size_t size();

    void flush();
    void Close();

private:
    deque<T> deq_;                    // 底层数据结构  deque：循环队列
    mutex mtx_;                       // 锁
    bool isClose_;                    // 关闭标志
    size_t capacity_;                 // 容量
    condition_variable condConsumer_; // 消费者条件变量
    condition_variable condProducer_; // 生产者条件变量
};

template <typename T>
BlockQueue<T>::BlockQueue(size_t maxsize) : capacity_(maxsize)
{
    assert(maxsize > 0);
    isClose_ = false;
}

template <typename T>
BlockQueue<T>::~BlockQueue()
{
    Close();
}

template <typename T>
void BlockQueue<T>::Close()
{
    // lock_guard<mutex> locker(mtx_);  //操控队列之前，都需要上锁
    // deq_.clear();                    //清空队列
    clear();
    isClose_ = true;
    /*调用条件变量 condConsumer_ 和 condProducer_ 的 notify_all() 方法，唤醒所有等待的线程。
    这样做是因为队列关闭后，任何试图从队列中获取或添加元素的操作都不应该继续等待，而应该立即返回。
     notify_all 函数的作用如下：
     1、唤醒所有线程：它会唤醒所有因调用 wait、wait_for 或 wait_until 而在此条件变量上等待的线程。
     2、互斥锁的重新获取：被唤醒的线程在重新尝试获取与条件变量关联的互斥锁之前，会一直处于阻塞状态。这是因为条件变量的 wait 系列函数在线程被唤醒时不会立即返回，而是会重新尝试获取互斥锁，只有成功获取互斥锁后才会返回。
     3、条件的重新检查：即使线程被唤醒，它也需要重新检查条件是否满足。这是因为在线程等待期间，条件可能已经发生了变化，或者可能有多个线程被唤醒，但只有条件真正满足的线程才会继续执行。*/
    condConsumer_.notify_all();
    condProducer_.notify_all();
}

template <typename T>
void BlockQueue<T>::clear()
{
    /*lock_guard的作用是管理一个互斥锁（mutex）的生命周期，确保在作用域开始时自动锁定互斥锁，并在作用域结束时自动解锁互斥锁。
    这样可以避免因异常或早期返回导致的死锁问题，提高代码的安全性和健壮性。
        void function() {
            std::lock_guard<std::mutex> locker(mtx_);
            // 临界区代码，mtx_ 在这里被锁定
            // ...
        }
    在上面的例子中，当 function 函数被调用时，lock_guard 对象 locker 被创建，并且 mtx_ 互斥锁被锁定。
    当 function 函数执行完毕，无论是正常返回还是因为异常而退出，locker 对象都会被析构，这时 mtx_ 互斥锁会被自动解锁。
    lock_guard 的特点包括：
    1、自动锁定和解锁：在构造时锁定互斥锁，在析构时解锁互斥锁。
    2、异常安全：即使在临界区代码抛出异常时，也能确保互斥锁被解锁，避免死锁。
    3、不可复制和不可赋值：lock_guard 对象不能被复制或赋值，这防止了互斥锁的不当管理。
    4、作用域限制：lock_guard 通常用于保护临界区代码，确保互斥锁的锁定和解锁与临界区代码的作用域紧密相关。*/
    lock_guard<mutex> locker(mtx_); // 操控队列之前，都需要上锁
    deq_.clear();                   // 清空队列
}

template <typename T>
bool BlockQueue<T>::empty()
{
    lock_guard<mutex> locker(mtx_);
    return deq_.empty();
}

template <typename T>
bool BlockQueue<T>::full()
{
    lock_guard<mutex> locker(mtx_);
    return deq_.size() >= capacity_;
}

template <typename T>
void BlockQueue<T>::push_back(const T &item)
{
    // 注意，条件变量需要搭配unique_lock
    unique_lock<mutex> locker(mtx_);
    while (deq_.size() >= capacity_)
    {                               // 队列满了，需要等待
        condProducer_.wait(locker); // 暂停生产，等待消费者唤醒生产条件变量
    }
    deq_.push_back(item);
    condConsumer_.notify_one(); // 唤醒消费者
}

template <typename T>
void BlockQueue<T>::push_front(const T &item)
{
    unique_lock<mutex> locker(mtx_);
    while (deq_.size() >= capacity_)
    {
        condProducer_.wait(locker);
    }
    deq_.push_front(item);
    condConsumer_.notify_one();
}

template <typename T>
bool BlockQueue<T>::pop(T &item)
{
    unique_lock<mutex> locker(mtx_);
    while (deq_.empty())
    {
        condConsumer_.wait(locker); // 队列空了，需要等待
    }
    item = deq_.front();
    deq_.pop_front();
    condProducer_.notify_one(); // 唤醒生产者
}

template <typename T>
bool BlockQueue<T>::pop(T &item, int timeout)
{
    // 使用 unique_lock<std::mutex> 来管理互斥锁 mtx_。unique_lock 比 lock_guard 更灵活，因为它可以手动锁定和解锁，并且可以在构造时不立即锁定。
    unique_lock<std::mutex> locker(mtx_);
    while (deq_.empty())
    {
        /*当前线程等待直到以下两种情况之一发生：
        1、条件变量 condConsumer_ 被另一个线程通过调用 notify_one 或 notify_all 唤醒。
        2、等待时间超过了指定的超时时间，这里是 timeout 秒。

        std::chrono::seconds(timeout) 是一个持续时间对象，表示等待的时长。
        std::cv_status::timeout 是一个枚举值，用于表示等待操作因为超时而结束。

        std::cv_status::timeout 表示等待操作因为超时而结束。
        std::cv_status::no_timeout 表示等待操作因为条件变量被唤醒而结束。
        */
        if (condConsumer_.wait_for(locker, std::chrono::seconds(timeout)) == std::cv_status::timeout)
        {
            return false;
        }
        if (isClose_)
        {
            return false;
        }
    }
    item = deq_.front();
    deq_.pop_front();
    condProducer_.notify_one();
    return true;
}

template <typename T>
T BlockQueue<T>::front()
{
    lock_guard<std::mutex> locker(mtx_);
    return deq_.front();
}

template <typename T>
T BlockQueue<T>::back()
{
    lock_guard<std::mutex> locker(mtx_);
    return deq_.back();
}

template <typename T>
size_t BlockQueue<T>::capacity()
{
    lock_guard<std::mutex> locker(mtx_);
    return capacity_;
}

template <typename T>
size_t BlockQueue<T>::size()
{
    lock_guard<std::mutex> locker(mtx_);
    return deq_.size();
}

// 唤醒消费者
template <typename T>
void BlockQueue<T>::flush()
{
    condConsumer_.notify_one();
}

#endif