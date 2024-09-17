#ifndef WEB_SERVER_H
#define WEB_SERVER_H

// 包含必要的头文件
#include <unordered_map> // 使用 unordered_map 容器
#include <fcntl.h>		 // fcntl() 函数
#include <unistd.h>		 // close() 函数
#include <assert.h>		 // assert() 函数
#include <errno.h>		 // errno 变量
#include <sys/socket.h>	 // socket 相关函数和结构体
#include <netinet/in.h>	 // sockaddr_in 结构体
#include <arpa/inet.h>	 // inet_pton() 函数

#include "epoller.h"			// 包含 epoller 类
#include "../timer/heaptimer.h" // 包含 HeapTimer 类

#include "../log/log.h"			 // 包含日志类
#include "../pool/sqlconnpool.h" // 包含 SQL 连接池类
#include "../pool/threadpool.h"	 // 包含线程池类

#include "../http/httpconn.h" // 包含 HTTP 连接类

// WebServer 类的定义
class WebServer
{
public:
	// 构造函数，初始化 WebServer 对象
	WebServer(
		int port, int trigMode, int timeoutMS,
		int sqlPort, const char *sqlUser, const char *sqlPwd,
		const char *dbName, int connPoolNum, int threadNum,
		bool openLog, int logLevel, int logQueSize);

	// 析构函数，销毁 WebServer 对象
	~WebServer();

	// 启动服务器
	void Start();

private:
	// 初始化套接字
	bool InitSocket_();

	// 初始化事件模式
	void InitEventMode_(int trigMode);

	// 添加客户端
	void AddClient_(int fd, sockaddr_in addr);

	// 处理监听事件
	void DealListen_();

	// 处理写事件
	void DealWrite_(HttpConn *client);

	// 处理读事件
	void DealRead_(HttpConn *client);

	// 发送错误信息
	void SendError_(int fd, const char *info);

	// 延长客户端连接时间
	void ExtentTime_(HttpConn *client);

	// 关闭客户端连接
	void CloseConn_(HttpConn *client);

	// 处理读事件
	void OnRead_(HttpConn *client);

	// 处理写事件
	void OnWrite_(HttpConn *client);

	// 处理客户端请求
	void OnProcess(HttpConn *client);

	// 最大文件描述符数量
	static const int MAX_FD = 65536;

	// 设置文件描述符为非阻塞模式
	static int SetFdNonblock(int fd);

	int port_;		  // 服务器端口号
	bool openLinger_; // 是否启用优雅关闭
	int timeoutMS_;	  // 超时时间（毫秒）
	bool isClose_;	  // 服务器是否关闭
	int listenFd_;	  // 监听套接字文件描述符
	char *srcDir_;	  // 资源目录

	uint32_t listenEvent_; // 监听事件类型
	uint32_t connEvent_;   // 连接事件类型

	std::unique_ptr<HeapTimer> timer_;		  // 定时器
	std::unique_ptr<ThreadPool> threadpool_;  // 线程池
	std::unique_ptr<Epoller> epoller_;		  // epoll 实例
	std::unordered_map<int, HttpConn> users_; // 客户端连接映射表
};

#endif // WEB_SERVER_H