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
    // 加上边沿触发ET
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;

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

    Init();
}

void HttpConn::Init()
{
    check_state_ = CHECK_STATE_REQUESTLINE; // 初始化状态为解析请求首行
    checked_index_ = 0;
    start_line_ = 0;
    read_index_ = 0;

    method_ = GET;
    url_ = nullptr;
    version_ = nullptr;

    memset(read_buf_, 0, read_buffer_size_);
    linger_ = false;
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
    // 循环读取客户端数据，直到无数据刻度或者对方连接关闭
    if (read_index_ >= read_buffer_size_)
    {
        return false;
    }

    // 读取到的字节
    int bytes_read = 0;
    while (true)
    {
        bytes_read = recv(sockfd_, read_buf_ + read_index_,
                          read_buffer_size_ - read_index_, 0);
        if (bytes_read == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // 没有数据
                break;
            }
            return false;
        }
        else if (bytes_read == 0)
        {
            // 对方关闭连接
            return false;
        }
        // 将索引往后移
        read_index_ += bytes_read;
    }
    printf("读取到的数据\n: %s\n", read_buf_);
    return true;
}

// 主状态机：解析请求
HttpConn::HTTP_CODE HttpConn::ProcessRead()
{
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;

    char *text = nullptr;
    // 解析了一行完整的数据，或者解析到了完整的请求体
    while (((check_state_ == CHECK_STATE_CONTENT) && (line_status == LINE_OK)) ||
           (line_status = ParseLine()) == LINE_OK)
    {
        text = GetLine();
        start_line_ = checked_index_;
        printf("got 1 http line : %s ", text);
        switch (check_state_)
        {
        // 解析请求行
        case CHECK_STATE_REQUESTLINE:
        {
            ret = ParseRequestLine(text);
            if (ret == BAD_REQUEST)
            {
                return BAD_REQUEST;
            }
            break;
        }
        // 解析请求头
        case CHECK_STATE_HEADER:
        {
            ret = ParseHeaders(text);
            if (ret == BAD_REQUEST)
            {
                return BAD_REQUEST;
            }
            else if (ret == GET_REQUEST)
            {
                return DoRequest();
            }
            break;
        }

        // 解析请求体
        case CHECK_STATE_CONTENT:
        {
            ret = ParseContent(text);
            if (ret == GET_REQUEST)
            {
                return DoRequest();
            }
            line_status = LINE_OPEN;
            break;
        }
        default:
        {
            return INTERNAL_ERROR;
        }
        }
    }

    return NO_REQUEST;
}

// 解析HTTP请求行，获得请求方法，目标URL，HTTP版本
HttpConn::HTTP_CODE HttpConn::ParseRequestLine(char *text)
{
    // GET /index.html HTTP/1.1
    url_ = strpbrk(text, " \t");
    *url_++ = '\0';
    // GET\0/index.html HTTP/1.1

    char *method = text; // 得到GET
    if (strcasecmp(method, "GET") == 0)
    {
        method_ = GET;
    }
    else
    {
        // 表示语法错误（还可能POST等）
        return BAD_REQUEST;
    }
    // /index.html HTTP/1.1
    version_ = strpbrk(url_, " \t");
    if (!version_)
    {
        return BAD_REQUEST;
    }
    // /index.html\0HTTP/1.1
    *version_++ = '\0';
    if (strcasecmp(version_, "HTTP/1.1") != 0)
    {
        return BAD_REQUEST;
    }
    // http://192.168.87.129:10000/index.html
    if (strncasecmp(url_, "http://", 7) == 0)
    {
        url_ += 7;          // 192.168.87.129:10000/index.html
        url_ = strchr(url_, '/');       // index.html
    }

    if(!url_ || url_[0] == '/') {
        return BAD_REQUEST;
    }

    check_state_ = CHECK_STATE_HEADER;      // 主状态机检查状态变成检查请求头
    return NO_REQUEST;
}

HttpConn::HTTP_CODE HttpConn::ParseHeaders(char *text) { return NO_REQUEST; }

HttpConn::HTTP_CODE HttpConn::ParseContent(char *text) { return NO_REQUEST; }

// 解析一行，判断的依据：\\r\n
HttpConn::LINE_STATUS HttpConn::ParseLine()
{

    char temp;
    for (; checked_index_ < read_index_; checked_index_++)
    {
        temp = read_buf_[checked_index_];
        if (temp == '\r')
        {
            if (checked_index_ + 1 == read_index_)
            {
                return LINE_OPEN;
            }
            else if (read_buf_[checked_index_ + 1] == '\n')
            {
                read_buf_[checked_index_++] = '\0';
                read_buf_[read_index_++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if (temp == '\n')
        {
            if ((checked_index_ > 1) && (read_buf_[checked_index_ - 1] == '\r'))
            {
                read_buf_[checked_index_ - 1] = '\0';
                read_buf_[checked_index_++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        return LINE_OPEN;
    }
    return LINE_OK;
}

HttpConn::HTTP_CODE HttpConn::DoRequest()
{
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
    HTTP_CODE read_ret = ProcessRead();
    if (read_ret == NO_REQUEST)
    {
        Modfd(epollfd_, sockfd_, EPOLLIN);
        return;
    }

    // 生成响应
}