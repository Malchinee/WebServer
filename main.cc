#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include "locker.h"
#include "threadpool.h"
#include "http_conn.h"
#include <assert.h>

// 最大的文件描述符的个数
const int max_fd = 65535;
// 监听的最大的事件数量
const int max_event_number = 1000;

// 添加信号捕捉
void addsig(int sig, void(handler)(int))
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    sigfillset(&sa.sa_mask); // 临时阻塞信号集
    assert(sigaction(sig, &sa, nullptr) != -1);
}

// 添加文件描述符到epoll中
extern void Addfd(int epollfd, int fd, bool one_shot);
// 从epoll中删除文件描述符
extern void Removefd(int epollfd, int fd);
// 修改文件描述符
extern void Modfd(int epollfd, int fd, int ev);

int main(int argc, char *argv[])
{
    if (argc <= 1)
    {
        printf("按照如下格式运行 : %s port_number\n", basename(argv[0]));
        exit(-1);
    }
    // 获取端口号
    int port = atoi(argv[1]);

    // 对SIGIPE信号进行处理
    addsig(SIGPIPE, SIG_IGN); // 忽略

    // 创建线程池,初始化线程池
    ThreadPool<HttpConn> *pool = nullptr;
    try
    {
        pool = new ThreadPool<HttpConn>;
    }
    catch (...)
    {
        exit(-1);
    }

    // 创建一个数组用于保存所有客户端的信息
    HttpConn *users = new HttpConn[max_fd];

    // 创建监听的套接字
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);

    // 设置端口复用
    int reuse = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    // 绑定
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    bind(listenfd, (struct sockaddr *)&address, sizeof(address));

    // 监听
    listen(listenfd, 5);

    // 创建epoll对象,事件数组，添加
    epoll_event events[max_event_number];
    int epollfd = epoll_create(5);

    // 将监听的文件描述符添加到epoll对象中
    Addfd(epollfd, listenfd, false);
    HttpConn::epollfd_ = epollfd;

    while (true)
    {
        int num = epoll_wait(epollfd, events, max_event_number, -1);
        if ((num < 0) && (errno != EINTR))
        {
            printf("epoll failure\n");
            break;
        }

        // 循环遍历事件数组
        for (int i = 0; i < num; i++)
        {
            int sockfd = events[i].data.fd;
            if (sockfd == listenfd)
            {
                // 有客户端连接进来
                struct sockaddr_in client_address;
                socklen_t client_addrlen = sizeof(client_address);
                int connfd = accept(listenfd, (struct sockaddr *)&client_address, &client_addrlen);

                if (HttpConn::user_count_ >= max_fd)
                {
                    // TODO:目前的连接数满了，应该给客户端提示服务器正忙
                    close(connfd);
                    continue;
                }

                // 将新的客户得数据初始化，放到数组中
                users[connfd].Init(connfd, client_address);
            }
            else if (events[i].events & (EPOLLRDHUP, EPOLLHUP | EPOLLERR))
            {
                // 对方异常断开或错误等事件
                // 关闭连接
                users[sockfd].CloseConn();
            }
            else if (events[i].events & EPOLLIN)
            { // 有读事件
                if (users[sockfd].Read())
                {
                    // 一次性把所有数据都读完
                    pool->Append(users + sockfd);
                }
                else
                {
                    users[sockfd].CloseConn();
                }
            }
            else if (events[i].events & EPOLLOUT)
            { // 有写事件
                if (!users[sockfd].Write())
                {
                    // 一次性写完所有数据
                    users[sockfd].CloseConn();
                }
            }
        }
    }
    close(epollfd);
    close(listenfd);
    delete[] users;
    delete pool;

    return 0;
}