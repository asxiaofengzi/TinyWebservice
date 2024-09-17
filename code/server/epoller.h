#ifndef EPOLLER_H
#define EPOLLER_H

#include <sys/epoll.h> //epoll_ctl()
#include <unistd.h>    // close()
#include <assert.h>    // close()
#include <vector>
#include <errno.h>

class Epoller
{
private:
  int epollFd_;                            // epoll句柄
  std::vector<struct epoll_event> events_; // 事件数组
public:
  explicit Epoller(int maxEvent = 1024); // 构造函数 maxEvent默认为1024
  ~Epoller();                            // 析构函数

  bool AddFd(int fd, uint32_t events); // 添加事件
  bool ModFd(int fd, uint32_t events); // 修改事件
  bool DelFd(int fd);                  // 删除事件
  int Wait(int timeoutMs = -1);        // 等待事件
  int GetEventFd(size_t i) const;      // 获取事件的fd
  uint32_t GetEvents(size_t i) const;  // 获取事件属性
};

#endif // EPOLLER_H