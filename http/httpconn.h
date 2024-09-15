#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <sys/types.h>
#include <sys/uio.h>     // readv/writev
#include <arpa/inet.h>   // sockaddr_in
#include <stdlib.h>      // atoi()
#include <errno.h>

#include "../log/log.h"
#include "../buffer/buffer.h"
#include "httprequest.h"
#include "httpresponse.h"
/*
进行读写数据并调用httprequest 来解析数据以及httpresponse来生成响应
*/
class HttpConn {
private:
	int fd_; // socket文件描述符
	struct sockaddr_in addr_; // 客户端地址

	bool isClose_; // 是否关闭连接

	int iovCnt_; // 读写缓冲区的数量
	struct iovec iov_[2]; // 读写缓冲区

	Buffer readBuff_; // 读缓冲区
	Buffer writeBuff_; // 写缓冲区

	HttpRequest request_; // 请求
	HttpResponse response_; // 响应
public:
	HttpConn();
	~HttpConn();

	void init(int sockFd, const sockaddr_in& addr);
	ssize_t read(int* saveErrno); // 读取数据
	ssize_t write(int* saveErrno); // 写入数据
	void Close(); // 关闭连接
	int GetFd() const; // 获取文件描述符
	int GetPort() const; // 获取端口
	const char* GetIP() const; // 获取IP地址
	sockaddr_in GetAddr() const; // 获取地址
	bool process(); // 处理请求

	// 写的总长度
	int ToWriteBytes() {
		return iov_[0].iov_len + iov_[1].iov_len; // 读写缓冲区的长度
	}
	bool IsKeepAlive() const {
		return request_.IsKeepAlive(); // 检查连接是否保持活动状态
	}

	static bool isET; // 是否使用ET模式
	static const char* srcDir; // 源目录
	static std::atomic<int> userCount; // 用户数量 原子操作
};

#endif // HTTP_CONN_H