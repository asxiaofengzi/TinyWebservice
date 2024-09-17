#include "httpconn.h"
using namespace std;

const char* HttpConn::srcDir; // 源目录
std::atomic<int> HttpConn::userCount; // 用户数量
bool HttpConn::isET; // 是否使用ET模式	

HttpConn::HttpConn() {
	fd_ = -1;
	addr_ = {0};
	isClose_ = true;
}

HttpConn::~HttpConn() {
	Close();
}

void HttpConn::init(int fd, const sockaddr_in& addr) {
	assert(fd > 0);
	userCount++; // 用户数量加1
	addr_ = addr; // 客户端地址
	fd_ = fd; // 文件描述符
	writeBuff_.RetrieveAll(); // 清空写缓冲区
	readBuff_.RetrieveAll(); // 清空读缓冲区
	isClose_ = false; // 连接未关闭
	LOG_INFO("Client[%d](%s:%d) in, userCount:%d", fd_, GetIP(), GetPort(), (int)userCount);
}

void HttpConn::Close() {
	response_.UnmapFile(); // 关闭文件映射
	if(isClose_ == false) {
		isClose_ = true; // 连接关闭
		userCount--; // 用户数量减1
		close(fd_); // 关闭文件描述符
		LOG_INFO("Client[%d](%s:%d) quit, UserCount:%d", fd_, GetIP(), GetPort(), (int)userCount);
	}
}

int HttpConn::GetFd() const {
	return fd_;
}

struct sockaddr_in HttpConn::GetAddr() const {
	return addr_;
}

const char* HttpConn::GetIP() const {
	/*inet_ntoa 是一个用于将网络字节序的 IPv4 地址转换为点分十进制字符串的函数。
	它通常用于将 struct in_addr 类型的地址转换为可读的字符串格式。*/
	return inet_ntoa(addr_.sin_addr);
}

int HttpConn::GetPort() const {
	return addr_.sin_port;
}

 // 读取数据
ssize_t HttpConn::read(int* saveErrno) {
	ssize_t len = -1;
	do {
		len = readBuff_.ReadFd(fd_, saveErrno); // 读取数据
		if(len <= 0) {
			break;
		}
	} while(isET); // 边缘触发模式 一次性全部读出
	return len;
}

// 写入数据 主要采用writev连续写函数
ssize_t HttpConn::write(int* saveErrno) {
	sseize_t len = -1; // 写入数据长度
	do {
		len = writev(fd_, iov_, iovCnt_); // 将iov的内容写到fd中
		if(len <= 0) {
			*saveErrno = errno; // 保存错误号
			break;
		}
		if(iov_[0].iov_len + iov_[1].iov_len == 0) { // 读写缓冲区的长度为0，退出
			break;
			//static_cast 是 C++ 中的一种类型转换运算符，用于在编译时执行类型转换。它比 C 风格的类型转换更安全，因为它提供了更严格的类型检查。
		} else if(static_cast<size_t>(len) > iov_[0].iov_len) {
			iov_[1].iov_base = (uint8_t*)iov_[1].iov_base + (len - iov_[0].iov_len); // 更新iov_[1]
			iov_[1].iov_len -= (len - iov_[0].iov_len); // 更新iov_[1]的长度
			if(iov_[0].iov_len) {
				writeBuff_.RetrieveAll(); // 清空写缓冲区
				iov_[0].iov_len = 0;
			}
		} else {
			iov_[0].iov_base = (uint8_t*)iov_[0].iov_base + len; // 更新iov_[0]
			iov_[0].iov_len -= len; // 更新iov_[0]的长度
		}
	}while(isET || ToWriteBytes() > 10240); // 边缘触发模式 或者 读写缓冲区的长度大于10240
	return len;
}
// 处理请求
bool HttpConn::process() {
	request_.Init(); // 初始化请求
	if(readBuff_.ReadableBytes() <= 0) { // 读缓冲区的长度小于等于0
		return false;
	}
	if(request_.Parse(readBuff_)) { // 解析请求
		LOG_DEBUG("%s", request_.path().c_str());
		response_.Init(srcDir, request_.path(), request_.IsKeepAlive(), 200); // 初始化响应
	} else {
		response_.Init(srcDir, request_.path(), false, 400); // 初始化响应
	}

	response_.MakeResponse(writeBuff_); // 生成响应
	// 响应头
	iov_[0].iov_base = const_cast<char*>(writeBuff_.Peek()); // 读写缓冲区的头
	iov_[0].iov_len = writeBuff_.ReadableBytes(); // 读写缓冲区的长度
	iovCnt_ = 1; // 读写缓冲区的数量

	// 文件
	if(response_.FileLen() > 0 && response_.File()) { // 文件长度大于0 并且 文件存在
		iov_[1].iov_base = response_.File(); // 读写缓冲区的第二部分
		iov_[1].iov_len = response_.FileLen(); // 读写缓冲区的第二部分的长度
		iovCnt_ = 2; // 读写缓冲区的数量
	}
	LOG_DEBUG("filesize:%d, %d to %d", response_.FileLen(), iovCnt_, ToWriteBytes());
	return true;
}
