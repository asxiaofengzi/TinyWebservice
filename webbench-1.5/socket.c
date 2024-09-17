/* $Id: socket.c 1.1 1995/01/01 07:11:14 cthuang Exp $
 *
 * This module has been modified by Radim Kolar for OS/2 emx
 */

/***********************************************************************
  module:       socket.c
  program:      popclient
  SCCS ID:      @(#)socket.c    1.5  4/1/94
  programmer:   Virginia Tech Computing Center
  compiler:     DEC RISC C compiler (Ultrix 4.1)
  environment:  DEC Ultrix 4.3
  description:  UNIX sockets code.
 ***********************************************************************/

#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

int Socket(const char *host, int clientPort)
{
    int sock;              // 定义套接字描述符
    unsigned long inaddr;  // 定义一个无符号长整型变量，用于存储 IP 地址
    struct sockaddr_in ad; // 定义一个 sockaddr_in 结构体变量，用于存储地址信息
    struct hostent *hp;    // 定义一个 hostent 结构体指针，用于存储主机信息

    // 将地址结构体 ad 清零
    memset(&ad, 0, sizeof(ad));

    // 设置地址族为 IPv4
    ad.sin_family = AF_INET;

    // 将主机名转换为网络字节序的 IP 地址
    inaddr = inet_addr(host);

    // 如果转换成功（不是 INADDR_NONE）
    if (inaddr != INADDR_NONE)
        // 将 IP 地址复制到地址结构体的 sin_addr 字段
        memcpy(&ad.sin_addr, &inaddr, sizeof(inaddr));
    else
    {
        // 如果转换失败，通过主机名获取主机信息
        hp = gethostbyname(host);

        // 如果获取主机信息失败，返回 -1 表示错误
        if (hp == NULL)
            return -1;

        // 将主机信息中的 IP 地址复制到地址结构体的 sin_addr 字段
        memcpy(&ad.sin_addr, hp->h_addr, hp->h_length);
    }

    // 设置端口号，使用网络字节序
    ad.sin_port = htons(clientPort);

    // 创建套接字，使用 IPv4 地址族，流式套接字，默认协议
    sock = socket(AF_INET, SOCK_STREAM, 0);

    // 如果创建套接字失败，返回套接字描述符（负值表示错误）
    if (sock < 0)
        return sock;

    // 连接到指定的主机和端口
    if (connect(sock, (struct sockaddr *)&ad, sizeof(ad)) < 0)
    {
        // 如果连接失败，关闭套接字并返回 -1 表示错误
        close(sock);
        return -1;
    }

    // 返回套接字描述符
    return sock;
}
