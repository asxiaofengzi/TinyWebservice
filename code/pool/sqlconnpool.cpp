#include "sqlconnpool.h"

/*定义了 SqlConnPool 类的静态成员函数 Instance。
该函数返回一个指向 SqlConnPool 对象的指针。
静态成员函数可以在没有类实例的情况下调用，并且通常用于实现单例模式。*/
SqlConnPool* SqlConnPool::Instance() {
	/*声明并定义了一个静态局部变量 pool，其类型为 SqlConnPool。
	静态局部变量在函数第一次调用时初始化，并在程序的整个生命周期内保持存在。
	由于它是静态的，这个变量只会被创建一次，确保了 SqlConnPool 类的唯一实例。*/
	static SqlConnPool pool;
	/*返回指向静态局部变量 pool 的指针。
	通过返回这个指针，Instance 方法提供了对 SqlConnPool 唯一实例的全局访问点。*/
	return &pool;
}

// 初始化连接池
/*用于初始化数据库连接池。它接受以下参数：

host: 数据库主机地址。
port: 数据库端口号。
user: 数据库用户名。
pwd: 数据库密码。
dbName: 数据库名称。
connSize: 连接池中连接的数量，默认为 10。
*/
void SqlConnPool::Init(const char* host, uint16_t port,
				const char* user, const char* pwd,
				const char* dbName, int connSize = 10) {
	assert(connSize > 0);
	// 创建指定数量的数据库连接。
	for(int i = 0; i < connSize; i++) {
		MYSQL* conn = nullptr; // 声明一个指向 MYSQL 类型的指针 conn，并将其初始化为 nullptr。
		conn = mysql_init(conn); // 调用 mysql_init 函数初始化 conn。如果初始化失败，conn 将保持为 nullptr。
		if(!conn) {
			LOG_ERROR("MySql init error!");
			assert(conn);
		}
		// 调用 mysql_real_connect 函数连接到数据库。该函数使用提供的主机、用户、密码、数据库名称和端口号。如果连接失败，conn 将为 nullptr。
		conn = mysql_real_connect(conn, host, user, pwd, dbName, port, nullptr, 0);
		if(!conn) {
			LOG_ERROR("MySql Connect error!");
		}
		// 将 conn 放入连接队列 connQue_ 中，即使连接失败也会放入队列。
		// emplace 是 C++ 标准库中容器类（如 std::queue, std::vector, std::map 等）提供的一个成员函数。
		//它用于在容器中原地构造元素，避免了不必要的拷贝或移动操作，从而提高性能。
		connQue_.emplace(conn);
	}
	MAX_CONN_ = connSize;
	// 初始化一个信号量 semId_，初始值为 MAX_CONN_。信号量用于控制对连接池的访问。
	sem_init(&semId_, 0 ,MAX_CONN_);
}

MYSQL* SqlConnPool::GetConn() {
	MYSQL* conn = nullptr; // 声明一个指向 MYSQL 类型的指针 conn，并将其初始化为 nullptr。
	// 如果连接队列 connQue_ 为空，表示没有可用连接，返回 nullptr。
	if(connQue_.empty()) {
		LOG_WARN("SqlConnPool busy!");
		return nullptr;
	}
	// 如果连接队列 connQue_ 不为空，从队列中取出一个连接。
	sem_wait(&semId_); // 等待信号量 semId_，如果信号量的值大于 0，将其减 1；否则阻塞当前线程。
	lock_guard<mutex> locker(mtx_); // 创建一个 lock_guard 对象 locker，用于在作用域结束时自动释放互斥锁 mtx_。
	conn = connQue_.front(); // 取出连接队列 connQue_ 的第一个元素，并将其赋值给 conn。
	connQue_.pop(); // 弹出连接队列 connQue_ 的第一个元素。
	return conn; // 返回取出的连接。
}

// 存入连接池，实际上没有关闭
void SqlConnPool::FreeConn(MYSQL* conn) {
	assert(conn); // 断言 conn 不为空。
	lock_guard<mutex> locker(mtx_); // 创建一个 lock_guard 对象 locker，用于在作用域结束时自动释放互斥锁 mtx_。
	connQue_.push(conn); // 将连接 conn 放回连接队列 connQue_。
	sem_post(&semId_); // 释放信号量 semId_，将其加 1。
}

// 关闭连接池
void SqlConnPool::ClosePool() {
	lock_guard<mutex> locker(mtx_); // 创建一个 lock_guard 对象 locker，用于在作用域结束时自动释放互斥锁 mtx_。
	while(!connQue_.empty()) { // 循环直到连接队列 connQue_ 为空。
		auto conn = connQue_.front(); // 取出连接队列 connQue_ 的第一个元素，并将其赋值给 conn。
		connQue_.pop(); // 弹出连接队列 connQue_ 的第一个元素。
		mysql_close(conn); // 关闭连接 conn。
	}
	mysql_library_end(); // 调用 mysql_library_end 函数，释放 MySQL 库的资源。
}

// 获取当前空闲连接的数量
int SqlConnPool::GetFreeConnCount() {
	lock_guard<mutex> locker(mtx_); // 创建一个 lock_guard 对象 locker，用于在作用域结束时自动释放互斥锁 mtx_。
	return connQue_.size(); // 返回连接队列 connQue_ 的大小，即当前空闲连接的数量。
}