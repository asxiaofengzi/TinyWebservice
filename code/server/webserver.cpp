#include "webserver.h"

using namespace std;

// WebServer 构造函数，初始化 WebServer 对象
WebServer::WebServer(
    int port, int trigMode, int timeoutMS,
    int sqlPort, const char *sqlUser, const char *sqlPwd,
    const char *dbName, int connPoolNum, int threadNum,
    bool openLog, int logLevel, int logQueSize)
    : port_(port),                            // 初始化服务器端口号
      timeoutMS_(timeoutMS),                  // 初始化超时时间（毫秒）
      isClose_(false),                        // 初始化服务器关闭标志为 false
      timer_(new HeapTimer()),                // 初始化定时器
      threadpool_(new ThreadPool(threadNum)), // 初始化线程池，指定线程数量
      epoller_(new Epoller())                 // 初始化 epoll 实例
{
    // 是否打开日志标志
    if (openLog)
    {
        // 初始化日志系统
        Log::Instance()->init(logLevel, "./log", ".log", logQueSize);

        // 如果服务器关闭标志为 true，记录错误日志
        if (isClose_)
        {
            LOG_ERROR("=========== Server init error!===========");
        }
        else
        {
            // 记录服务器初始化成功日志
            LOG_INFO("=========== Server init ============");

            // 记录监听模式和连接模式
            LOG_INFO("Listen Mode: %s, OpenConn Mode: %s",
                     (listenEvent_ & EPOLLET ? "ET" : "LT"), // 监听模式：边缘触发（ET）或水平触发（LT）
                     (connEvent_ & EPOLLET ? "ET" : "LT"));  // 连接模式：边缘触发（ET）或水平触发（LT）

            // 记录日志系统级别
            LOG_INFO("LogSys Level: %d", logLevel);

            // 记录资源目录
            LOG_INFO("srcDir: %s", HttpConn::srcDir);

            // 记录 SQL 连接池数量和线程池数量
            LOG_INFO("sqlConnPool num: %d, ThreadPool num: %d", connPoolNum, threadNum);
        }
    }

    srcDir_ = getcwd(nullptr, 256); // 获取当前工作目录
    assert(srcDir_);                // 断言当前工作目录不为空
    strcat(srcDir_, "/resources/"); // 拼接资源目录
    HttpConn::userCount = 0;        // 初始化用户数量为 0
    HttpConn::srcDir = srcDir_;     // 设置资源目录

    // 初始化 SQL 连接池
    SqlConnPool::Instance()->Init("localhost", sqlPort, sqlUser, sqlPwd, dbName, connPoolNum); // 连接池单例的初始化
    // 初始化事件模式和初始化套接字（监听）
    InitEventMode_(trigMode); // 初始化事件模式
    if (!InitSocket_())
    {
        isClose_ = true;
    } // 初始化套接字
}

// WebServer 析构函数，销毁 WebServer 对象
WebServer::~WebServer()
{
    close(listenFd_);                     // 关闭监听套接字
    isClose_ = true;                      // 设置服务器关闭标志为 true
    free(srcDir_);                        // 释放资源目录
    SqlConnPool::Instance()->ClosePool(); // 关闭 SQL 连接池
}

// 初始化事件模式
void WebServer::InitEventMode_(int trigMode)
{
    listenEvent_ = EPOLLRDHUP;              // 监听事件类型：检测 socket 关闭
    connEvent_ = EPOLLONESHOT | EPOLLRDHUP; // 连接事件类型：EPOLLONESHOT 由一个线程处理

    /*在代码中，|= 运算符用于将某些标志位添加到现有的事件标志中。例如：
    connEvent_ |= EPOLLET;
    这行代码的作用是将 EPOLLET 标志添加到 connEvent_ 中。
    假设 connEvent_ 原本的值是 EPOLLONESHOT | EPOLLRDHUP，
    那么执行这行代码后，connEvent_ 的值将变成 EPOLLONESHOT | EPOLLRDHUP | EPOLLET。*/
    switch (trigMode)
    {
    case 0: // 不做任何修改，保持默认设置。 水平触发模式
        break;
    case 1:
        connEvent_ |= EPOLLET; // 将 connEvent_ 设置为边缘触发模式（EPOLLET）。
        break;
    case 2:
        listenEvent_ |= EPOLLET; // 将 listenEvent_ 设置为边缘触发模式（EPOLLET）。
        break;
    case 3:
        // 将 listenEvent_ 和 connEvent_ 都设置为边缘触发模式（EPOLLET）。
        listenEvent_ |= EPOLLET; // 边缘触发模式
        connEvent_ |= EPOLLET;   // 边缘触发模式
        break;
    default:
        // 如果 trigMode 不在上述范围内，默认将 listenEvent_ 和 connEvent_ 都设置为边缘触发模式（EPOLLET）。
        listenEvent_ |= EPOLLET; // 边缘触发模式
        connEvent_ |= EPOLLET;   // 边缘触发模式
        break;
    }
    HttpConn::isET = (connEvent_ & EPOLLET); // 设置连接事件类型
}

// 启动服务器
void WebServer::Start()
{
    // 初始化时间变量，设置为 -1 表示 epoll_wait 将无限期阻塞，直到有事件发生
    int timeMS = -1;

    // 如果服务器未关闭，记录服务器启动日志
    if (!isClose_)
    {
        LOG_INFO("========== Server start ==========");
    }

    // 进入主循环，直到服务器关闭
    while (!isClose_)
    {
        // 如果设置了超时时间
        if (timeoutMS_ > 0)
        {
            // 获取下一次的超时等待时间
            timeMS = timer_->GetNextTick();
        }

        // 调用 epoll 等待事件，返回事件数量
        int eventCnt = epoller_->Wait(timeMS);

        // 遍历所有事件
        for (int i = 0; i < eventCnt; i++)
        {
            // 获取事件的文件描述符
            int fd = epoller_->GetEventFd(i);

            // 获取事件类型
            uint32_t events = epoller_->GetEvents(i);

            // 如果事件是监听套接字的事件
            if (fd == listenFd_)
            {
                // 处理监听事件（如新连接）
                DealListen_();
            }
            // 如果事件是关闭、挂起或错误事件
            else if (events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                // 确保用户映射表中存在该文件描述符
                assert(users_.count(fd) > 0);

                // 关闭客户端连接
                CloseConn_(&users_[fd]);
            }
            // 如果事件是读事件
            else if (events & EPOLLIN)
            {
                // 确保用户映射表中存在该文件描述符
                assert(users_.count(fd) > 0);

                // 处理读事件
                DealRead_(&users_[fd]);
            }
            // 如果事件是写事件
            else if (events & EPOLLOUT)
            {
                // 确保用户映射表中存在该文件描述符
                assert(users_.count(fd) > 0);

                // 处理写事件
                DealWrite_(&users_[fd]);
            }
            // 如果是其他未预期的事件，记录错误日志
            else
            {
                LOG_ERROR("Unexpected event");
            }
        }
    }
}

// 发送错误信息
void WebServer::SendError_(int fd, const char *info)
{
    // 断言文件描述符有效（大于 0）
    assert(fd > 0);

    // 发送错误信息给客户端
    // fd 是客户端的文件描述符
    // info 是要发送的错误信息
    // strlen(info) 获取错误信息的长度
    // 0 表示默认的发送标志
    int ret = send(fd, info, strlen(info), 0);

    // 如果发送失败（返回值小于 0）
    if (ret < 0)
    {
        // 记录警告日志，提示发送错误信息失败
        LOG_WARN("send error to client[%d] error!", fd);
    }

    // 关闭客户端的文件描述符
    close(fd);
}

// 关闭客户端连接
void WebServer::CloseConn_(HttpConn *client)
{
    // 断言客户端连接有效
    assert(client);

    // 记录客户端退出日志
    LOG_INFO("Client[%d] quit!", client->GetFd());

    // 从 epoll 实例中删除客户端的文件描述符
    epoller_->DelFd(client->GetFd());

    // 关闭客户端连接
    client->Close();
}

// 添加客户端
void WebServer::AddClient_(int fd, sockaddr_in addr)
{
    // 断言文件描述符有效
    assert(fd > 0);

    // 初始化客户端连接
    users_[fd].init(fd, addr);

    // 如果设置了超时时间
    if (timeoutMS_ > 0)
    {
        // 添加定时器，超时时间为 timeoutMS_ 毫秒
        timer_->add(fd, timeoutMS_, std::bind(&WebServer::CloseConn_, this, &users_[fd]));
    }

    // 向 epoll 实例中添加客户端的文件描述符和事件类型
    // fd 是客户端的文件描述符
    // EPOLLIN | connEvent_ 是事件类型，包括读事件和连接事件
    epoller_->AddFd(fd, EPOLLIN | connEvent_);

    // 设置文件描述符为非阻塞模式
    SetFdNonblock(fd);

    // 记录客户端连接日志
    LOG_INFO("Client[%d] in!", users_[fd].GetFd());
}

// 处理监听套接字,主要逻辑是accept新的套接字，并加入timer和epoller中
void WebServer::DealListen_()
{
    // 定义客户端地址结构体
    struct sockaddr_in addr;

    // 定义客户端地址结构体长度
    socklen_t len = sizeof(addr);

    // 循环处理新连接
    do
    {
        // 接受新的连接，返回客户端的文件描述符
        int fd = accept(listenFd_, (struct sockaddr *)&addr, &len);

        // 如果文件描述符小于等于 0，返回
        if (fd <= 0)
        {
            return;
        }

        // 如果客户端数量大于等于最大文件描述符数量
        else if (HttpConn::userCount >= MAX_FD)
        {
            // 发送错误信息给客户端
            SendError_(fd, "Server busy!");

            // 记录警告日志，提示客户端数量已满
            LOG_WARN("Clients is full!");

            // 返回
            return;
        }

        // 添加客户端
        AddClient_(fd, addr);
    } while (listenEvent_ & EPOLLET); // 如果是边缘触发模式，继续循环
}

// 处理读事件，主要逻辑是将 OnRead 加入线程池的任务队列中
void WebServer::DealRead_(HttpConn *client)
{
    // 断言客户端连接有效
    assert(client);

    // 延长客户端连接时间
    ExtentTime_(client);

    // 将 OnRead 加入线程池的任务队列中
    threadpool_->AddTask(std::bind(&WebServer::OnRead_, this, client));
}

// 处理写事件，主要逻辑是将 OnWrite 加入线程池的任务队列中
void WebServer::DealWrite_(HttpConn *client)
{
    // 断言客户端连接有效
    assert(client);

    // 延长客户端连接时间
    ExtentTime_(client);

    // 将 OnWrite 加入线程池的任务队列中
    threadpool_->AddTask(std::bind(&WebServer::OnWrite_, this, client));
}

// 延长客户端连接时间
void WebServer::ExtentTime_(HttpConn *client)
{
    // 断言客户端连接有效
    assert(client);

    // 如果设置了超时时间
    if (timeoutMS_ > 0)
    {
        // 调整定时器，将客户端的文件描述符和超时时间传入
        timer_->adjust(client->GetFd(), timeoutMS_);
    }
}

// 处理读事件
void WebServer::OnRead_(HttpConn *client)
{
    // 断言客户端连接有效
    assert(client);

    // 定义读取返回值和读取错误号
    int ret = -1;
    int readErrno = 0;

    // 读取客户端套接字的数据，读到 httpconn 的读缓存区
    ret = client->read(&readErrno);

    // 如果读取返回值小于等于 0 并且读取错误号不是 EAGAIN
    if (ret <= 0 && readErrno != EAGAIN)
    {
        // 关闭客户端连接
        CloseConn_(client);

        // 返回
        return;
    }

    // 业务逻辑的处理（先读后处理）
    OnProcess(client);
}

// 处理读（请求）数据的函数
void WebServer::OnProcess(HttpConn *client)
{
    // 首先调用 process() 进行逻辑处理
    if (client->process())
    {
        // 读完事件就跟内核说可以写了
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT); // 响应成功，修改监听事件为写,等待 OnWrite_() 发送
    }
    else
    {
        // 写完事件就跟内核说可以读了
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLIN);
    }
}

// 处理写事件
void WebServer::OnWrite_(HttpConn *client)
{
    // 断言客户端连接有效
    assert(client);

    // 定义写返回值和写错误号
    int ret = -1;
    int writeErrno = 0;

    // 写数据到客户端套接字
    ret = client->write(&writeErrno);

    // 如果客户端要写的字节数为 0
    if (client->ToWriteBytes() == 0)
    {
        // 传输完成
        if (client->IsKeepAlive())
        {
            // OnProcess(client);
            epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLIN); // 回归换成监测读事件
            return;
        }
    }
    // 如果写返回值小于 0
    else if (ret < 0)
    {
        // 如果写错误号是 EAGAIN，缓冲区满了
        if (writeErrno == EAGAIN)
        {
            // 继续传输
            // 传输完成
            if (client->IsKeepAlive())
            {
                // OnProcess(client);
                epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLIN); // 回归换成监测读事件
                return;
            }
        }
    }
    CloseConn_(client); // 关闭客户端连接
}

// 创建监听套接字
bool WebServer::InitSocket_()
{
    int ret;                 // 用于存储函数返回值的变量
    struct sockaddr_in addr; // 定义一个 sockaddr_in 结构体变量，用于存储地址信息

    // 设置地址族为 IPv4
    addr.sin_family = AF_INET;

    // 设置 IP 地址为本地所有 IP 地址
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // 设置端口号，使用网络字节序
    addr.sin_port = htons(port_);

    // 创建套接字，使用 IPv4 地址族，流式套接字，默认协议
    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);

    // 如果创建套接字失败，记录错误日志并返回 false
    if (listenFd_ < 0)
    {
        LOG_ERROR("Create socket error!", port_);
        return false;
    }

    int optval = 1; // 设置套接字选项的值为 1

    // 设置套接字选项，允许端口复用
    // 只有最后一个套接字会正常接收数据
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));

    // 如果设置套接字选项失败，记录错误日志，关闭套接字并返回 false
    if (ret == -1)
    {
        LOG_ERROR("set socket setsockopt error !");
        close(listenFd_);
        return false;
    }

    // 绑定套接字到指定的 IP 地址和端口号
    ret = bind(listenFd_, (struct sockaddr *)&addr, sizeof(addr));

    // 如果绑定失败，记录错误日志，关闭套接字并返回 false
    if (ret < 0)
    {
        LOG_ERROR("Bind Port:%d error!", port_);
        close(listenFd_);
        return false;
    }

    // 开始监听，最大监听队列长度为 8
    ret = listen(listenFd_, 8);

    // 如果监听失败，记录错误日志，关闭套接字并返回 false
    if (ret < 0)
    {
        LOG_ERROR("Listen port:%d error!", port_);
        close(listenFd_);
        return false;
    }

    // 将监听套接字加入 epoller，监听读事件
    ret = epoller_->AddFd(listenFd_, listenEvent_ | EPOLLIN);

    // 如果添加到 epoller 失败，记录错误日志，关闭套接字并返回 false
    if (ret == 0)
    {
        LOG_ERROR("Add listen error!");
        close(listenFd_);
        return false;
    }

    // 设置监听套接字为非阻塞模式
    SetFdNonblock(listenFd_);

    // 记录服务器端口信息
    LOG_INFO("Server port:%d", port_);

    // 返回 true，表示初始化成功
    return true;
}

// 设置文件描述符为非阻塞模式
int WebServer::SetFdNonblock(int fd)
{
    // 断言文件描述符有效（大于 0）
    assert(fd > 0);

    // 获取文件描述符的当前标志，并添加非阻塞标志
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}
