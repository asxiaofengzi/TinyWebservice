#include "heaptimer.h"

#include "heaptimer.h"  // 包含头文件

void HeapTimer::SwapNode_(size_t i, size_t j) {
    assert(i >= 0 && i <heap_.size());  // 确保索引 i 合法
    assert(j >= 0 && j <heap_.size());  // 确保索引 j 合法
    swap(heap_[i], heap_[j]);  // 交换堆中两个节点的位置
    ref_[heap_[i].id] = i;  // 更新节点 i 的索引
    ref_[heap_[j].id] = j;  // 更新节点 j 的索引
}

void HeapTimer::siftup_(size_t i) {
    assert(i >= 0 && i < heap_.size());  // 确保索引 i 合法
    size_t parent = (i - 1) / 2;  // 计算父节点的索引
    while (parent >= 0) {
        if (heap_[parent] > heap_[i]) {  // 如果父节点大于当前节点
            SwapNode_(i, parent);  // 交换当前节点和父节点
            i = parent;  // 更新当前节点索引为父节点索引
            parent = (i - 1) / 2;  // 重新计算父节点索引
        } else {
            break;  // 如果父节点不大于当前节点，跳出循环
        }
    }
}

// false：不需要下滑  true：下滑成功
bool HeapTimer::siftdown_(size_t i, size_t n) {
    assert(i >= 0 && i < heap_.size());  // 确保索引 i 合法
    assert(n >= 0 && n <= heap_.size());  // 确保 n 合法
    auto index = i;  // 当前节点索引
    auto child = 2 * index + 1;  // 左子节点索引
    while (child < n) {
        if (child + 1 < n && heap_[child + 1] < heap_[child]) {
            child++;  // 如果右子节点小于左子节点，选择右子节点
        }
        if (heap_[child] < heap_[index]) {
            SwapNode_(index, child);  // 交换当前节点和子节点
            index = child;  // 更新当前节点索引为子节点索引
            child = 2 * child + 1;  // 重新计算子节点索引
        } else {
            break;  // 如果子节点不小于当前节点，跳出循环
        }
    }
    return index > i;  // 返回是否进行了下滑操作
}

// 删除指定位置的结点
void HeapTimer::del_(size_t index) {
    assert(index >= 0 && index < heap_.size());  // 确保索引合法
    size_t tmp = index;  // 临时变量保存索引
    size_t n = heap_.size() - 1;  // 最后一个节点的索引
    assert(tmp <= n);  // 确保临时变量索引合法
    if (index < heap_.size() - 1) {
        SwapNode_(tmp, heap_.size() - 1);  // 将要删除的节点换到队尾
        if (!siftdown_(tmp, n)) {
            siftup_(tmp);  // 调整堆
        }
    }
    ref_.erase(heap_.back().id);  // 删除映射
    heap_.pop_back();  // 删除队尾节点
}

// 调整指定id的结点
void HeapTimer::adjust(int id, int newExpires) {
    assert(!heap_.empty() && ref_.count(id));  // 确保堆不为空且 id 存在
    heap_[ref_[id]].expires = Clock::now() + MS(newExpires);  // 更新超时时间
    siftdown_(ref_[id], heap_.size());  // 向下调整堆
}

void HeapTimer::add(int id, int timeOut, const TimeoutCallBack& cb) {
    assert(id >= 0);  // 确保 id 合法
    if (ref_.count(id)) {
        int tmp = ref_[id];  // 获取 id 对应的索引
        heap_[tmp].expires = Clock::now() + MS(timeOut);  // 更新超时时间
        heap_[tmp].cb = cb;  // 更新回调函数
        if (!siftdown_(tmp, heap_.size())) {
            siftup_(tmp);  // 调整堆
        }
    } else {
        size_t n = heap_.size();  // 获取当前堆的大小
        ref_[id] = n;  // 将 id 映射到新的索引
        heap_.push_back({id, Clock::now() + MS(timeOut), cb});  // 添加新节点
        siftup_(n);  // 向上调整堆
    }
}

// 删除指定id，并触发回调函数
void HeapTimer::doWork(int id) {
    if (heap_.empty() || ref_.count(id) == 0) {
        return;  // 如果堆为空或 id 不存在，直接返回
    }
    size_t i = ref_[id];  // 获取 id 对应的索引
    auto node = heap_[i];  // 获取节点
    node.cb();  // 触发回调函数
    del_(i);  // 删除节点
}

void HeapTimer::tick() {
    /* 清除超时结点 */
    if (heap_.empty()) {
        return;  // 如果堆为空，直接返回
    }
    while (!heap_.empty()) {
        TimerNode node = heap_.front();  // 获取堆顶节点
        if (std::chrono::duration_cast<MS>(node.expires - Clock::now()).count() > 0) {
            break;  // 如果堆顶节点未超时，跳出循环
        }
        node.cb();  // 触发回调函数
        pop();  // 移除堆顶节点
    }
}

void HeapTimer::pop() {
    assert(!heap_.empty());  // 确保堆不为空
    del_(0);  // 删除堆顶节点
}

void HeapTimer::clear() {
    ref_.clear();  // 清空映射
    heap_.clear();  // 清空堆
}

int HeapTimer::GetNextTick() {
    tick();  // 处理所有已超时的定时器
    size_t res = -1;  // 初始化返回值
    if (!heap_.empty()) {
        res = std::chrono::duration_cast<MS>(heap_.front().expires - Clock::now()).count();  // 计算下一个定时器的超时时间
        if (res < 0) { res = 0; }  // 如果超时时间小于0，设置为0
    }
    return res;  // 返回下一个定时器的超时时间
}