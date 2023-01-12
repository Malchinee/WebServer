#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H
#include "locker.h"
#include <arpa/inet.h>
#include <error.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <cstring>

class HttpConn {
public:
  static int epollfd_; // 所有socket上的事件都被注册到同一个epoll_fd中
  static int user_count_;                     // 统计用户的数量
  static const int read_buffer_size_ = 2048;  // 读缓冲区的大小
  static const int write_buffer_size_ = 1024; // 写缓冲区的大小

  enum METHOD // HTTP报文请求头的方法
  {
    GET = 0,
    POST,
    HEAD,
    PUT,
    DELETE,
    TRACE,
    OPTIONS,
    CONNECT
  };

  enum CHECK_STATE // 解析客户端请求时，主状态机的状态
  {
    CHECK_STATE_REQUESTLINE = 0, // 当前正在分析请求行
    CHECK_STATE_HEADER,          // 当前正在分析头部字段
    CHECK_STATE_CONTENT          // 当前正在解析请求体
  };

  enum LINE_STATUS // 从状态机的三种可能状态，即行的读取状态
  {
    LINE_OK = 0, // 读取到一个完整的行
    LINE_BAD,    // 行出错
    LINE_OPEN    // 行数据尚且不完整
  };

  enum HTTP_CODE // 服务器处理HTTP请求的可能结果，报文解析结果
  {
    NO_REQUEST,        // 请求不完整，需要继续读取客户数据
    GET_REQUEST,       // 表示获得了一个完成的客户请求
    BAD_REQUEST,       // 客户请求语法错误
    NO_RESOURCE,       // 服务器没有资源
    FORBIDDEN_REQUEST, // 客户对资源没有足够的访问权限
    FILE_REQUEST,      // 文件请求，获取文件成功
    INTERNAL_ERROR,    // 服务器内部错误
    CLOSE_CONNECTION   // 客户端已经关闭连接
  };

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

  char read_buf_[read_buffer_size_]; // 读缓冲区
  int read_index_; // 标识读缓冲区中已经读入客户端数据的最后一个字节的下一位

  int checked_index_; // 当前正在分析的字符在读缓冲区的位置
  int start_line_;    // 当前正在解析的行的起始位置

  char * url_;    // 请求目标文件的文件名
  char * version_;    // 协议版本，只支持HTTP1.1
  METHOD method_;     // 请求方法

  char * host_;     // 主机名
  bool linger_;     // 判断HTTP请求是否要连接

  CHECK_STATE check_state_; // 主状态机当前所处的状态

  void Init(); // 初始化其他信息

  HTTP_CODE ProcessRead();                // 解析HTTP请求
  HTTP_CODE ParseRequestLine(char *text); // 解析请求首行
  HTTP_CODE ParseHeaders(char *text);     // 解析请求头
  HTTP_CODE ParseContent(char *text);     // 解析请求内容

  LINE_STATUS ParseLine(); // 解析一行

  char *GetLine() { return read_buf_ + start_line_; }

  HTTP_CODE DoRequest();        // 做具体的处理
};

#endif
