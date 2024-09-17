#ifndef SQLCONNPOOL_H
#define SQLCONNPOOL_H

#include <mysql/mysql.h>
#include <string>
#include <queue>
#include <mutex>
#include <semaphore.h>
#include <thread>
#include "../log/log.h"

// SqlConnPool 类是一个用于管理 MySQL 数据库连接池的单例类。它提供了一些方法来初始化连接池、获取和释放连接，以及关闭连接池。
class SqlConnPool
{
private:
	// 单例模式确保一个类只有一个实例，避免了创建多个实例带来的资源浪费和不一致性问题。
	// 单例模式提供一个全局访问点，允许程序的不同部分访问同一个实例。
	// 通过 Instance() 方法，可以在任何地方获取到同一个实例，确保全局状态的一致性。
	/*通过以下方式实现单例模式：

	私有构造函数和析构函数：

	将构造函数和析构函数声明为私有，防止外部代码直接创建或销毁实例。
	这样，外部代码无法通过 new 或 delete 操作符创建或销毁 SqlConnPool 的实例。
	静态实例方法：

	提供一个静态方法 Instance()，用于返回类的唯一实例。
	该方法内部通常会检查实例是否已经创建，如果没有创建则创建一个新的实例，并返回该实例。*/
	SqlConnPool() = default;				// 私有构造函数，确保只能通过 Instance() 方法获取实例。这是为了实现单例模式
	~SqlConnPool() { ClosePool(); } // 析构函数，在对象销毁时关闭连接池

	int MAX_CONN_; // 最大连接数

	std::queue<MYSQL *> connQue_; // 连接队列，存储可用的 MySQL 连接
	std::mutex mtx_;							// 互斥锁，用于保护连接队列的访问
	// sem_t 是 POSIX 信号量的类型，用于在多线程编程中实现线程同步。信号量是一个计数器，用于控制对共享资源的访问。
	sem_t semId_; // 信号量，用于管理连接池中的可用连接数

public:
	static SqlConnPool *Instance(); // 获取单例实例的方法

	MYSQL *GetConn();						// 获取一个可用的 MySQL 连接
	void FreeConn(MYSQL *conn); // 释放一个 MySQL 连接，将其归还到连接池
	int GetFreeConnCount();			// 获取当前空闲连接的数量

	void Init(const char *host, uint16_t port,
						const char *user, const char *pwd,
						const char *dbName, int connSize); // 初始化连接池
	void ClosePool();														 // 关闭连接池，释放所有连接
};

/*资源在对象构造初始化 资源在对象析构时释放
用于管理 MySQL 数据库连接的类，采用了 RAII（Resource Acquisition Is Initialization）模式。
RAII 是一种常见的 C++ 编程惯用法，通过在对象的生命周期内自动管理资源，确保资源的正确释放。*/
class SqlConnRAII
{
private:
	MYSQL *sql_;						// 用于存储从连接池获取的 MySQL 连接
	SqlConnPool *connpool_; // 指向一个连接池对象。
public:
	SqlConnRAII(MYSQL **sql, SqlConnPool *connpool)
	{
		assert(connpool_);
		// 从连接池中获取一个 MySQL 连接，并将其赋值给 sql 和 sql_
		*sql = connpool_->GetConn();
		sql_ = *sql;
		connpool_ = connpool;
	}

	/*在对象销毁时调用。如果 sql_ 非空，析构函数会将 MySQL 连接归还给连接池。
	这确保了在 SqlConnRAII 对象的生命周期结束时，MySQL 连接能够被正确释放，避免资源泄漏。*/
	~SqlConnRAII()
	{
		if (sql_)
		{
			connpool_->FreeConn(sql_);
		}
	}
};

#endif