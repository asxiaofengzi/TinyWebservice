#include "epoller.h"

// 构造函数，初始化 Epoller 对象
Epoller::Epoller(int maxEvent)
    // 初始化 epoll 文件描述符，创建一个 epoll 实例，参数 512 是提示内核需要的文件描述符数量
    : epollFd_(epoll_create(512)),
      // 初始化事件数组，大小为 maxEvent
      events_(maxEvent)
{
  // 断言检查，确保 epoll 文件描述符创建成功且事件数组大小大于 0
  assert(epollFd_ >= 0 && events_.size() > 0);
}

Epoller::~Epoller()
{
  // 关闭 epoll 文件描述符
  close(epollFd_);
}

// 向 epoll 实例中添加一个文件描述符及其事件
bool Epoller::AddFd(int fd, uint32_t events)
{
  // 如果文件描述符无效（小于 0），返回 false
  if (fd < 0)
    return false;

  // 初始化 epoll_event 结构体，清零所有成员
  epoll_event ev = {0};

  // 设置事件类型，例如 EPOLLIN, EPOLLOUT 等
  ev.events = events;

  // 调用 epoll_ctl 函数将文件描述符添加到 epoll 实例中
  // epollFd_ 是 epoll 实例的文件描述符
  // EPOLL_CTL_ADD 表示添加操作
  // fd 是要添加的文件描述符
  // &ev 是指向 epoll_event 结构体的指针，包含事件类型
  return 0 == epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev);
}

// 修改 epoll 实例中已存在的文件描述符的事件
bool Epoller::ModFd(int fd, uint32_t events)
{
  // 如果文件描述符无效（小于 0），返回 false
  if (fd < 0)
    return false;

  // 初始化 epoll_event 结构体，清零所有成员
  epoll_event ev = {0};

  // 设置 epoll_event 结构体中的文件描述符
  ev.data.fd = fd;

  // 设置事件类型，例如 EPOLLIN, EPOLLOUT 等
  ev.events = events;

  // 调用 epoll_ctl 函数修改 epoll 实例中已存在的文件描述符的事件
  // epollFd_ 是 epoll 实例的文件描述符
  // EPOLL_CTL_MOD 表示修改操作
  // fd 是要修改的文件描述符
  // &ev 是指向 epoll_event 结构体的指针，包含新的事件类型
  return 0 == epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &ev);
}

// 从 epoll 实例中删除一个文件描述符
bool Epoller::DelFd(int fd)
{
  // 如果文件描述符无效（小于 0），返回 false
  if (fd < 0)
    return false;

  // 调用 epoll_ctl 函数从 epoll 实例中删除文件描述符
  // epollFd_ 是 epoll 实例的文件描述符
  // EPOLL_CTL_DEL 表示删除操作
  // fd 是要删除的文件描述符
  // 最后一个参数为 0，因为删除操作不需要 epoll_event 结构体
  return 0 == epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, 0);
}

// 等待事件
int Epoller::Wait(int timeoutMs)
{
  // 调用 epoll_wait 函数等待事件
  // epollFd_ 是 epoll 实例的文件描述符
  // &events_[0] 是指向 epoll_event 结构体数组的指针
  // events_.size() 是 epoll_event 结构体数组的大小
  // timeoutMs 是超时时间，单位是毫秒
  return epoll_wait(epollFd_, &events_[0], static_cast<int>(events_.size()), timeoutMs);
}

// 获取事件的fd
int Epoller::GetEventFd(size_t i) const
{
  // 断言检查，确保 i 在合法范围内
  assert(i < events_.size() && i >= 0);

  // 返回第 i 个事件的文件描述符
  return events_[i].data.fd;
}

// 获取事件属性
uint32_t Epoller::GetEvents(size_t i) const
{
  // 断言检查，确保 i 在合法范围内
  assert(i < events_.size() && i >= 0);

  // 返回第 i 个事件的属性
  return events_[i].events;
}