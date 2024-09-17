/*
 * (C) Radim Kolar 1997-2004
 * This is free software, see GNU Public License version 2 for
 * details.
 *
 * Simple forking WWW Server benchmark:
 *
 * Usage:
 *   webbench --help
 *
 * Return codes:
 *    0 - sucess
 *    1 - benchmark failed (server is not on-line)
 *    2 - bad param
 *    3 - internal error, fork failed
 *
 */
#include "socket.c"    // 包含自定义的 socket.c 文件，用于处理套接字操作
#include <unistd.h>    // 包含 POSIX 操作系统 API 函数，如 read、write、close 等
#include <sys/param.h> // 包含系统参数和限制的定义
#include <rpc/types.h> // 包含远程过程调用（RPC）类型定义
#include <getopt.h>    // 包含命令行参数解析函数
#include <strings.h>   // 包含字符串操作函数，如 bzero 等
#include <time.h>      // 包含时间相关函数，如 time、clock 等
#include <signal.h>    // 包含信号处理函数，如 signal、raise 等

/* values */
volatile int timerexpired = 0; // 定义一个易变的全局变量，用于标记定时器是否到期
int speed = 0;                 // 定义一个全局变量，用于记录请求的速度
int failed = 0;                // 定义一个全局变量，用于记录失败的请求数
int bytes = 0;                 // 定义一个全局变量，用于记录传输的字节数

/* globals */
int http10 = 1; // 定义一个全局变量，用于标记 HTTP 版本（0 - HTTP/0.9, 1 - HTTP/1.0, 2 - HTTP/1.1）
/* Allow: GET, HEAD, OPTIONS, TRACE */
#define METHOD_GET 0          // 定义宏，表示 GET 方法
#define METHOD_HEAD 1         // 定义宏，表示 HEAD 方法
#define METHOD_OPTIONS 2      // 定义宏，表示 OPTIONS 方法
#define METHOD_TRACE 3        // 定义宏，表示 TRACE 方法
#define PROGRAM_VERSION "1.5" // 定义宏，表示程序版本号
int method = METHOD_GET;      // 定义一个全局变量，表示默认的 HTTP 方法为 GET
int clients = 1;              // 定义一个全局变量，表示默认的客户端数量为 1
int force = 0;                // 定义一个全局变量，用于标记是否强制执行
int force_reload = 0;         // 定义一个全局变量，用于标记是否强制重新加载
int proxyport = 80;           // 定义一个全局变量，表示代理服务器的默认端口号为 80
char *proxyhost = NULL;       // 定义一个全局变量，表示代理服务器的主机名，默认为空
int benchtime = 30;           // 定义一个全局变量，表示默认的测试时间为 30 秒

/* internal */
int mypipe[2];              // 定义一个全局数组，用于存储管道的文件描述符
char host[MAXHOSTNAMELEN];  // 定义一个全局字符数组，用于存储主机名
#define REQUEST_SIZE 2048   // 定义宏，表示请求的最大大小为 2048 字节
char request[REQUEST_SIZE]; // 定义一个全局字符数组，用于存储 HTTP 请求

// 定义一个静态的结构体数组，用于存储命令行选项
static const struct option long_options[] = {
    {"force", no_argument, &force, 1},                 // 定义 --force 选项，不需要参数，设置 force 变量为 1
    {"reload", no_argument, &force_reload, 1},         // 定义 --reload 选项，不需要参数，设置 force_reload 变量为 1
    {"time", required_argument, NULL, 't'},            // 定义 --time 选项，需要参数，设置选项字符为 't'
    {"help", no_argument, NULL, '?'},                  // 定义 --help 选项，不需要参数，设置选项字符为 '?'
    {"http09", no_argument, NULL, '9'},                // 定义 --http09 选项，不需要参数，设置选项字符为 '9'
    {"http10", no_argument, NULL, '1'},                // 定义 --http10 选项，不需要参数，设置选项字符为 '1'
    {"http11", no_argument, NULL, '2'},                // 定义 --http11 选项，不需要参数，设置选项字符为 '2'
    {"get", no_argument, &method, METHOD_GET},         // 定义 --get 选项，不需要参数，设置 method 变量为 METHOD_GET
    {"head", no_argument, &method, METHOD_HEAD},       // 定义 --head 选项，不需要参数，设置 method 变量为 METHOD_HEAD
    {"options", no_argument, &method, METHOD_OPTIONS}, // 定义 --options 选项，不需要参数，设置 method 变量为 METHOD_OPTIONS
    {"trace", no_argument, &method, METHOD_TRACE},     // 定义 --trace 选项，不需要参数，设置 method 变量为 METHOD_TRACE
    {"version", no_argument, NULL, 'V'},               // 定义 --version 选项，不需要参数，设置选项字符为 'V'
    {"proxy", required_argument, NULL, 'p'},           // 定义 --proxy 选项，需要参数，设置选项字符为 'p'
    {"clients", required_argument, NULL, 'c'},         // 定义 --clients 选项，需要参数，设置选项字符为 'c'
    {NULL, 0, NULL, 0}                                 // 定义数组结束标志
};

/* prototypes */
// 定义函数原型，用于声明在文件中使用的静态函数
static void benchcore(const char *host, const int port, const char *request); // 声明 benchcore 函数，处理基准测试的核心逻辑
static int bench(void);                                                       // 声明 bench 函数，执行基准测试
static void build_request(const char *url);                                   // 声明 build_request 函数，构建 HTTP 请求

// 定义一个静态函数，用于处理定时器信号
static void alarm_handler(int signal)
{
   timerexpired = 1; // 设置全局变量 timerexpired 为 1，表示定时器已到期
}

// 定义一个静态函数，用于显示程序的使用说明
static void usage(void)
{
   // 输出使用说明到标准错误输出
   fprintf(stderr,
           "webbench [option]... URL\n"
           "  -f|--force               Don't wait for reply from server.\n"
           "  -r|--reload              Send reload request - Pragma: no-cache.\n"
           "  -t|--time <sec>          Run benchmark for <sec> seconds. Default 30.\n"
           "  -p|--proxy <server:port> Use proxy server for request.\n"
           "  -c|--clients <n>         Run <n> HTTP clients at once. Default one.\n"
           "  -9|--http09              Use HTTP/0.9 style requests.\n"
           "  -1|--http10              Use HTTP/1.0 protocol.\n"
           "  -2|--http11              Use HTTP/1.1 protocol.\n"
           "  --get                    Use GET request method.\n"
           "  --head                   Use HEAD request method.\n"
           "  --options                Use OPTIONS request method.\n"
           "  --trace                  Use TRACE request method.\n"
           "  -?|-h|--help             This information.\n"
           "  -V|--version             Display program version.\n");
};

// 主函数，程序入口
int main(int argc, char *argv[])
{
   int opt = 0;           // 定义变量 opt，用于存储命令行选项
   int options_index = 0; // 定义变量 options_index，用于存储长选项索引
   char *tmp = NULL;      // 定义字符指针 tmp，用于临时存储字符串

   // 如果没有提供命令行参数，显示使用说明并返回 2
   if (argc == 1)
   {
      usage();
      return 2;
   }

   // 解析命令行参数
   while ((opt = getopt_long(argc, argv, "912Vfrt:p:c:?h", long_options, &options_index)) != EOF)
   {
      switch (opt)
      {
      case 0:
         break; // 如果选项为 0，直接跳过
      case 'f':
         force = 1;
         break; // 如果选项为 'f'，设置 force 变量为 1
      case 'r':
         force_reload = 1;
         break; // 如果选项为 'r'，设置 force_reload 变量为 1
      case '9':
         http10 = 0;
         break; // 如果选项为 '9'，设置 http10 变量为 0（使用 HTTP/0.9）
      case '1':
         http10 = 1;
         break; // 如果选项为 '1'，设置 http10 变量为 1（使用 HTTP/1.0）
      case '2':
         http10 = 2;
         break; // 如果选项为 '2'，设置 http10 变量为 2（使用 HTTP/1.1）
      case 'V':
         printf(PROGRAM_VERSION "\n");
         exit(0); // 如果选项为 'V'，显示程序版本并退出
      case 't':
         benchtime = atoi(optarg);
         break; // 如果选项为 't'，将 optarg 转换为整数并赋值给 benchtime
      case 'p':
         /* 解析代理服务器地址和端口 */
         tmp = strrchr(optarg, ':'); // 查找最后一个冒号的位置
         proxyhost = optarg;         // 设置代理服务器主机名
         if (tmp == NULL)
         {
            break; // 如果没有找到冒号，跳过
         }
         if (tmp == optarg)
         {
            fprintf(stderr, "Error in option --proxy %s: Missing hostname.\n", optarg); // 如果冒号在字符串开头，显示错误信息并返回 2
            return 2;
         }
         if (tmp == optarg + strlen(optarg) - 1)
         {
            fprintf(stderr, "Error in option --proxy %s Port number is missing.\n", optarg); // 如果冒号在字符串末尾，显示错误信息并返回 2
            return 2;
         }
         *tmp = '\0'; // 将冒号替换为字符串结束符
         proxyport = atoi(tmp + 1);
         break; // 将冒号后面的字符串转换为整数并赋值给 proxyport
      case ':':
      case 'h':
      case '?':
         usage();
         return 2;
         break; // 如果选项为 ':'、'h' 或 '?'，显示使用说明并返回 2
      case 'c':
         clients = atoi(optarg);
         break; // 如果选项为 'c'，将 optarg 转换为整数并赋值给 clients
      }
   }

   // 如果没有提供 URL，显示错误信息和使用说明并返回 2
   if (optind == argc)
   {
      fprintf(stderr, "webbench: Missing URL!\n");
      usage();
      return 2;
   }

   // 如果 clients 为 0，设置为 1
   if (clients == 0)
      clients = 1;
   // 如果 benchtime 为 0，设置为 60
   if (benchtime == 0)
      benchtime = 60;
   /* 显示版权信息 */
   fprintf(stderr, "Webbench - Simple Web Benchmark " PROGRAM_VERSION "\n"
                   "Copyright (c) Radim Kolar 1997-2004, GPL Open Source Software.\n");
   build_request(argv[optind]); // 构建 HTTP 请求
   /* 打印基准测试信息 */
   printf("\nBenchmarking: ");
   switch (method)
   {
   case METHOD_GET:
   default:
      printf("GET");
      break; // 如果方法为 GET 或默认，打印 "GET"
   case METHOD_OPTIONS:
      printf("OPTIONS");
      break; // 如果方法为 OPTIONS，打印 "OPTIONS"
   case METHOD_HEAD:
      printf("HEAD");
      break; // 如果方法为 HEAD，打印 "HEAD"
   case METHOD_TRACE:
      printf("TRACE");
      break; // 如果方法为 TRACE，打印 "TRACE"
   }
   printf(" %s", argv[optind]); // 打印 URL
   switch (http10)
   {
   case 0:
      printf(" (using HTTP/0.9)");
      break; // 如果使用 HTTP/0.9，打印 "(using HTTP/0.9)"
   case 2:
      printf(" (using HTTP/1.1)");
      break; // 如果使用 HTTP/1.1，打印 "(using HTTP/1.1)"
   }
   printf("\n");
   if (clients == 1)
      printf("1 client"); // 如果 clients 为 1，打印 "1 client"
   else
      printf("%d clients", clients); // 否则打印客户端数量

   printf(", running %d sec", benchtime); // 打印运行时间
   if (force)
      printf(", early socket close"); // 如果 force 为 1，打印 ", early socket close"
   if (proxyhost != NULL)
      printf(", via proxy server %s:%d", proxyhost, proxyport); // 如果使用代理服务器，打印代理服务器信息
   if (force_reload)
      printf(", forcing reload"); // 如果 force_reload 为 1，打印 ", forcing reload"
   printf(".\n");
   return bench(); // 执行基准测试并返回结果
}

// 构建 HTTP 请求
void build_request(const char *url)
{
   char tmp[10]; // 定义临时字符数组，用于存储端口号
   int i;        // 定义循环变量

   bzero(host, MAXHOSTNAMELEN);  // 将 host 数组清零
   bzero(request, REQUEST_SIZE); // 将 request 数组清零

   // 如果 force_reload 为 1 且使用代理服务器且 HTTP 版本小于 1，设置 HTTP 版本为 1
   if (force_reload && proxyhost != NULL && http10 < 1)
      http10 = 1;
   // 如果方法为 HEAD 且 HTTP 版本小于 1，设置 HTTP 版本为 1
   if (method == METHOD_HEAD && http10 < 1)
      http10 = 1;
   // 如果方法为 OPTIONS 且 HTTP 版本小于 2，设置 HTTP 版本为 2
   if (method == METHOD_OPTIONS && http10 < 2)
      http10 = 2;
   // 如果方法为 TRACE 且 HTTP 版本小于 2，设置 HTTP 版本为 2
   if (method == METHOD_TRACE && http10 < 2)
      http10 = 2;

   // 根据方法设置请求行
   switch (method)
   {
   default:
   case METHOD_GET:
      strcpy(request, "GET");
      break; // 如果方法为 GET 或默认，设置请求行为 "GET"
   case METHOD_HEAD:
      strcpy(request, "HEAD");
      break; // 如果方法为 HEAD，设置请求行为 "HEAD"
   case METHOD_OPTIONS:
      strcpy(request, "OPTIONS");
      break; // 如果方法为 OPTIONS，设置请求行为 "OPTIONS"
   case METHOD_TRACE:
      strcpy(request, "TRACE");
      break; // 如果方法为 TRACE，设置请求行为 "TRACE"
   }

   strcat(request, " "); // 在请求行后添加空格

   // 如果 URL 中不包含 "://"
   if (NULL == strstr(url, "://"))
   {
      fprintf(stderr, "\n%s: is not a valid URL.\n", url); // 打印错误信息
      exit(2);                                             // 退出程序，返回 2
   }
   // 如果 URL 长度超过 1500
   if (strlen(url) > 1500)
   {
      fprintf(stderr, "URL is too long.\n"); // 打印错误信息
      exit(2);                               // 退出程序，返回 2
   }
   // 如果不使用代理服务器且 URL 不以 "http://" 开头
   if (proxyhost == NULL)
      if (0 != strncasecmp("http://", url, 7))
      {
         fprintf(stderr, "\nOnly HTTP protocol is directly supported, set --proxy for others.\n"); // 打印错误信息
         exit(2);                                                                                  // 退出程序，返回 2
      }
   /* 协议/主机分隔符 */
   i = strstr(url, "://") - url + 3; // 计算主机名的起始位置
   /* printf("%d\n",i); */

   // 如果 URL 中不包含 '/'，打印错误信息并退出
   if (strchr(url + i, '/') == NULL)
   {
      fprintf(stderr, "\nInvalid URL syntax - hostname don't ends with '/'.\n");
      exit(2);
   }
   // 如果不使用代理服务器
   if (proxyhost == NULL)
   {
      /* 从主机名中获取端口号 */
      if (index(url + i, ':') != NULL &&
          index(url + i, ':') < index(url + i, '/'))
      {
         strncpy(host, url + i, strchr(url + i, ':') - url - i);                                // 复制主机名
         bzero(tmp, 10);                                                                        // 清零临时数组
         strncpy(tmp, index(url + i, ':') + 1, strchr(url + i, '/') - index(url + i, ':') - 1); // 复制端口号
         /* printf("tmp=%s\n",tmp); */
         proxyport = atoi(tmp); // 将端口号转换为整数
         if (proxyport == 0)
            proxyport = 80; // 如果端口号为 0，设置为 80
      }
      else
      {
         strncpy(host, url + i, strcspn(url + i, "/")); // 复制主机名
      }
      // printf("Host=%s\n",host);
      strcat(request + strlen(request), url + i + strcspn(url + i, "/")); // 在请求行后添加 URL
   }
   else
   {
      // printf("ProxyHost=%s\nProxyPort=%d\n",proxyhost,proxyport);
      strcat(request, url); // 在请求行后添加 URL
   }
   // 根据 HTTP 版本添加协议版本
   if (http10 == 1)
      strcat(request, " HTTP/1.0");
   else if (http10 == 2)
      strcat(request, " HTTP/1.1");
   strcat(request, "\r\n"); // 在请求行后添加回车换行符
   if (http10 > 0)
      strcat(request, "User-Agent: WebBench " PROGRAM_VERSION "\r\n"); // 添加 User-Agent 头
   if (proxyhost == NULL && http10 > 0)
   {
      strcat(request, "Host: "); // 添加 Host 头
      strcat(request, host);     // 添加主机名
      strcat(request, "\r\n");   // 添加回车换行符
   }
   if (force_reload && proxyhost != NULL)
   {
      strcat(request, "Pragma: no-cache\r\n"); // 添加 Pragma 头
   }
   if (http10 > 1)
      strcat(request, "Connection: close\r\n"); // 添加 Connection 头
   /* 在末尾添加空行 */
   if (http10 > 0)
      strcat(request, "\r\n");
   // printf("Req=%s\n",request);
}
/* vraci system rc error kod */
// 定义一个静态函数 bench，用于执行基准测试并返回系统错误代码
static int bench(void)
{
   int i, j, k;   // 定义三个整型变量，用于循环和存储结果
   pid_t pid = 0; // 定义一个进程 ID 变量，初始化为 0
   FILE *f;       // 定义一个文件指针变量

   /* check avaibility of target server */
   // 检查目标服务器的可用性
   i = Socket(proxyhost == NULL ? host : proxyhost, proxyport);
   if (i < 0)
   {
      // 如果连接服务器失败，打印错误信息并返回 1
      fprintf(stderr, "\nConnect to server failed. Aborting benchmark.\n");
      return 1;
   }
   close(i); // 关闭套接字

   /* create pipe */
   // 创建管道，用于进程间通信
   if (pipe(mypipe))
   {
      // 如果创建管道失败，打印错误信息并返回 3
      perror("pipe failed.");
      return 3;
   }

   /* not needed, since we have alarm() in childrens */
   /* wait 4 next system clock tick */
   /*
   cas = time(NULL);
   while (time(NULL) == cas)
      sched_yield();
   */

   /* fork childs */
   // 创建子进程
   for (i = 0; i < clients; i++)
   {
      pid = fork();
      if (pid <= (pid_t)0)
      {
         // 如果是子进程或出错，休眠 1 秒以加快子进程的启动速度
         sleep(1);
         break;
      }
   }

   if (pid < (pid_t)0)
   {
      // 如果 fork 失败，打印错误信息并返回 3
      fprintf(stderr, "problems forking worker no. %d\n", i);
      perror("fork failed.");
      return 3;
   }

   if (pid == (pid_t)0)
   {
      // 如果是子进程
      if (proxyhost == NULL)
         benchcore(host, proxyport, request); // 执行基准测试核心逻辑
      else
         benchcore(proxyhost, proxyport, request);

      /* write results to pipe */
      // 将结果写入管道
      f = fdopen(mypipe[1], "w");
      if (f == NULL)
      {
         // 如果打开管道写端失败，打印错误信息并返回 3
         perror("open pipe for writing failed.");
         return 3;
      }
      // 将速度、失败次数和字节数写入管道
      fprintf(f, "%d %d %d\n", speed, failed, bytes);
      fclose(f); // 关闭文件指针
      return 0;  // 子进程退出
   }
   else
   {
      // 如果是父进程
      f = fdopen(mypipe[0], "r");
      if (f == NULL)
      {
         // 如果打开管道读端失败，打印错误信息并返回 3
         perror("open pipe for reading failed.");
         return 3;
      }
      setvbuf(f, NULL, _IONBF, 0); // 设置文件流为无缓冲模式
      speed = 0;                   // 初始化速度为 0
      failed = 0;                  // 初始化失败次数为 0
      bytes = 0;                   // 初始化字节数为 0

      while (1)
      {
         // 从管道中读取速度、失败次数和字节数
         pid = fscanf(f, "%d %d %d", &i, &j, &k);
         if (pid < 2)
         {
            // 如果读取失败，打印错误信息并退出循环
            fprintf(stderr, "Some of our childrens died.\n");
            break;
         }
         speed += i;  // 累加速度
         failed += j; // 累加失败次数
         bytes += k;  // 累加字节数
         if (--clients == 0)
            break; // 如果所有客户端都已完成，退出循环
      }
      fclose(f); // 关闭文件指针

      // 打印基准测试结果
      printf("\nSpeed=%d pages/min, %d bytes/sec.\nRequests: %d succeed, %d failed.\n",
             (int)((speed + failed) / (benchtime / 60.0f)),
             (int)(bytes / (float)benchtime),
             speed,
             failed);
   }
   return i; // 返回最后一次读取的结果
}

// 定义一个函数 benchcore，用于执行基准测试的核心逻辑
void benchcore(const char *host, const int port, const char *req)
{
   int rlen;            // 定义一个整型变量，用于存储请求的长度
   char buf[1500];      // 定义一个字符数组，用于存储响应数据
   int s, i;            // 定义两个整型变量，用于存储套接字和读取结果
   struct sigaction sa; // 定义一个 sigaction 结构体变量，用于设置信号处理

   /* setup alarm signal handler */
   // 设置定时器信号处理函数
   sa.sa_handler = alarm_handler;
   sa.sa_flags = 0;
   if (sigaction(SIGALRM, &sa, NULL))
      exit(3);       // 如果设置信号处理失败，退出程序
   alarm(benchtime); // 设置定时器

   rlen = strlen(req); // 获取请求的长度
nexttry:
   while (1)
   {
      if (timerexpired)
      {
         // 如果定时器到期
         if (failed > 0)
         {
            // 如果有失败的请求，减少失败次数
            failed--;
         }
         return; // 返回
      }
      s = Socket(host, port); // 创建套接字并连接到服务器
      if (s < 0)
      {
         failed++;
         continue;
      } // 如果连接失败，增加失败次数并继续
      if (rlen != write(s, req, rlen))
      {
         failed++;
         close(s);
         continue;
      } // 如果写入请求失败，增加失败次数并关闭套接字
      if (http10 == 0)
         if (shutdown(s, 1))
         {
            failed++;
            close(s);
            continue;
         } // 如果使用 HTTP/0.9，关闭写端
      if (force == 0)
      {
         /* read all available data from socket */
         // 读取所有可用的数据
         while (1)
         {
            if (timerexpired)
               break;               // 如果定时器到期，退出循环
            i = read(s, buf, 1500); // 读取数据
            if (i < 0)
            {
               // 如果读取失败，增加失败次数并关闭套接字
               failed++;
               close(s);
               goto nexttry; // 跳转到 nexttry 标签
            }
            else if (i == 0)
               break; // 如果读取到的数据长度为 0，退出循环
            else
               bytes += i; // 累加读取到的字节数
         }
      }
      if (close(s))
      {
         failed++;
         continue;
      }        // 关闭套接字，如果失败，增加失败次数并继续
      speed++; // 增加成功请求次数
   }
}