#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H
#include <sys/epoll.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <error.h>
#include "locker.h"
#include <sys/uio.h>

class HttpConn
{
public:
    static int epollfd_;    // 所有socket上的事件都被注册到同一个epoll_fd中
    static int user_count_; // 统计用户的数量
    HttpConn() {}
    
    ~HttpConn() {}

    void Process(); // 处理客户端的请求

    void Init(int sockfd, sockaddr_in &addr); // 初始化新接收的连接

    void CloseConn(); // 关闭连接

    bool Read(); // 非阻塞的读

    bool Write(); // 非阻塞的写
private:
    int sockfd_;          // 该http连接的socket
    sockaddr_in address_; // 通信socket地址
};

#endif