#include "http_conn.h"

int HttpConn::epollfd_ = -1;
int HttpConn::user_count_ = 0;

// 设置文件描述符非阻塞
void setnonblocking(int fd)
{
    int old_flag = fcntl(fd, F_GETFL);
    int new_flag = old_flag | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_flag);
}

// 向epoll中添加需要监听的文件描述符
void Addfd(int epollfd, int fd, bool one_shot)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLRDHUP;

    if (one_shot)
    {
        event.events | EPOLLONESHOT;
    }

    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);

    // 设置文件描述符非阻塞
    setnonblocking(fd);
}

// 从epoll中移除监听的文件描述符
void Removefd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

// 修改文件描述符、重置socket上的EPOLLONESHOT事件，确保下一次可读时，EPOLLIN事件能被触发
void Modfd(int epollfd, int fd, int ev)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

// 初始化连接
void HttpConn::Init(int sockfd, sockaddr_in &addr)
{
    sockfd_ = sockfd;
    address_ = addr;

    // 设置端口复用
    int reuse = 1;
    setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    // 添加到epoll对象中
    Addfd(epollfd_, sockfd_, true);
    user_count_++; // 总用户数+1
}

// 关闭连接
void HttpConn::CloseConn()
{
    if (sockfd_ != -1)
    {
        Removefd(epollfd_, sockfd_);
        sockfd_ = -1;
        user_count_--; // 关闭一个连接，客户总数量-1
    }
}

bool HttpConn::Read()
{
    printf("一次性读完数据\n");
    return true;
}
bool HttpConn::Write()
{
    printf("一次性写完数据\n");
    return true;
}

// 由线程池中的工作线程调用，处理http请求的入口函数
void HttpConn::Process()
{
    // 解析http请求报文

    printf("parse request, create response\n");
    // 生成响应
}