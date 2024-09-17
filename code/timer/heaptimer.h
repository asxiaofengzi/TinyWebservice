#ifndef HEAP_TIMER_H
#define HEAP_TIMER_H

#include <queue>                // 包含队列容器的定义
#include <unordered_map>        // 包含哈希表容器的定义
#include <time.h>               // 包含时间相关函数的定义
#include <algorithm>            // 包含常用算法的定义，如排序、查找等
#include <arpa/inet.h>          // 包含网络相关函数的定义
#include <functional>           // 包含函数对象和回调函数的定义
#include <assert.h>             // 包含断言宏的定义，用于调试
#include <chrono>               // 包含时间库的定义，用于处理时间点和时间段
#include "../log/log.h"         // 包含自定义日志库的定义

typedef std::function<void()> TimeoutCallBack;  // 定义一个回调函数类型，表示超时后要执行的函数
typedef std::chrono::high_resolution_clock Clock;  // 定义一个高分辨率时钟类型
typedef std::chrono::milliseconds MS;  // 定义一个表示毫秒的时间段类型
typedef Clock::time_point TimeStamp;  // 定义一个时间点类型

struct TimerNode {
    int id;  // 定时器的唯一标识符
    TimeStamp expires;  // 超时时间点
    TimeoutCallBack cb; // 回调函数，超时后要执行的操作
    bool operator<(const TimerNode& t) {    // 重载小于运算符，用于比较两个定时器节点的超时时间
        return expires < t.expires;
    }
    bool operator>(const TimerNode& t) {    // 重载大于运算符，用于比较两个定时器节点的超时时间
        return expires > t.expires;
    }
};

class HeapTimer {
public:
    HeapTimer() { heap_.reserve(64); }  // 构造函数，初始化堆的容量为64
    ~HeapTimer() { clear(); }  // 析构函数，清空所有定时器
    
    void adjust(int id, int newExpires);  // 调整定时器的超时时间
    void add(int id, int timeOut, const TimeoutCallBack& cb);  // 添加一个新的定时器
    void doWork(int id);  // 执行定时器的回调函数
    void clear();  // 清空所有定时器
    void tick();  // 处理所有已超时的定时器
    void pop();  // 移除堆顶的定时器
    int GetNextTick();  // 获取下一个定时器的超时时间

private:
    void del_(size_t i);  // 删除指定位置的定时器
    void siftup_(size_t i);  // 向上调整堆
    bool siftdown_(size_t i, size_t n);  // 向下调整堆
    void SwapNode_(size_t i, size_t j);  // 交换两个定时器节点的位置

    std::vector<TimerNode> heap_;  // 存储定时器节点的堆
    std::unordered_map<int, size_t> ref_;  // 存储定时器 ID 和堆中位置的映射，方便查找
};

#endif //HEAP_TIMER_H