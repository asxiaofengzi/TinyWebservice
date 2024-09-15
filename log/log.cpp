#include "log.h"

//构造函数
Log::Log() {
	fp_ = nullptr;           //文件指针
	deque_ = nullptr;        //阻塞队列
	writeThread_ = nullptr;  //写线程的指针
	lineCount_ = 0;
	toDay_ = 0;
	isAsync_ = false;
}

Log::~Log() {
	while(!deque_->empty()) {
		deque_->flush();   //唤醒消费者，处理掉剩下的任务
	}
	deque_->Close();  //关闭队列
	writeThread_->join();  //等待当前线程完成手中的任务
	if(fp_) {  //冲洗文件缓冲区，关闭文件描述符
		lock_guard<mutex> locker(mtx_);
		flush();  //清空缓冲区中的数据
		fclose(fp_);  //关闭日志文件
	}
}
// 唤醒阻塞队列消费者，开始写日志
void Log::flush() {
	if(isAsync_) {  // 只有异步日志才会用到deque
		deque_->flush();
	}
	fflush(fp_);  // 清空输入缓冲区
}

// 懒汉模式 局部静态变量发（这种方法不需要加锁和解锁操作）
Log* Log::Instance() {
	static Log log;
	return &log;
}

// 异步日志的写线程函数
void Log::FlushLogThread() {
	Log::Instance()->AsyncWrite_();
}

// 写线程真正的执行函数
void Log::AsyncWrite_() {
	string str = "";
	while(deque_->pop(str)) {
		lock_guard<mutex> locker(mtx_);
		fputs(str.c_str(), fp_);
	}
}

// 初始化日志实例
void Log::init(int level, const char* path, const char* suffix, int maxQueCapacity) {
	isOpen_ = true;
	level_ = level;
	path_ = path;
	suffix_ = suffix;
	if(maxQueCapacity) {  // 异步方式
		isAsync_ = true;
		if(!deque_) { // 为空则创建一个
			unique_ptr<BlockQueue<std::string>> newQue(new BlockQueue<std::string>);
			// 因为unique_ptr不支持普通的拷贝或赋值操作，所以采用move
			// 将动态申请的内存权给deque，newDeque被释放
			deque_ = move(newQue);  // 左值变右值，掏空newDeque

			unique_ptr<thread> newThread(new thread(FlushLogThread));
			writeThread_ = move(newThread);
		}
	} else {
		isAsync_ = false;
	}

	lineCount_ = 0;

	time_t timer = time(nullptr);
	struct tm *sysTime = localtime(&timer);
	struct tm t = *sysTime;
	path_ = path;
	suffix_ = suffix;
	char fileName[LOG_NAME_LEN] = {0};
	snprintf(fileName, LOG_NAME_LEN - 1, "%s/%04d_%02d_%02d%s",
			path_, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, suffix_);
	toDay_ = t.tm_mday;

	{
		lock_guard<mutex> locker(mtx_);
		buff_.RetrieveAll();
		if(fp_) {  //重新打开
			flush();
			fclose(fp_);
		}
		fp_ = fopen(fileName, "a");  // 打开文件读取并附加写入
		if(fp_ == nullptr) {
			mkdir(path_, 0777);
			fp_ = fopen(fileName, "a");  // 生成目录文件（最大权限）
		}
		assert(fp_ != nullptr);
	}
}

