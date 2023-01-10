#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>
#include <unistd.h>
#include <list>
#include "locker.h"
#include <semaphore>
#include <cstdio>

// 线程池类、定义成模板类是为了代码的复用
template <typename T>
class ThreadPool
{
public:
    ThreadPool(int thread_number = 8, int max_request = 10000);
    ~ThreadPool();
    // 向线程池中添加任务
    bool Append(T *request);

private:
    static void *Worker(void *arg);
    void Run();

private:
    // 线程的数量
    int thread_number_;

    // 线程池数组，大小为thread_number_
    pthread_t *threads_;

    // 请求队列中最多允许的，等待处理的请求数量
    int max_requests_;

    // 请求队列
    std::list<T *> workqueue_;

    // 互斥锁
    Locker queuelocker_;

    // 信号量，用来判断是否有任务需要处理
    Sem queuestat_;

    // 是否结束线程
    bool stop_;
};
template <typename T>
ThreadPool<T>::ThreadPool(int thread_number, int max_requests) : thread_number_(thread_number), max_requests_(max_requests),
                                                                 stop_(false), threads_(nullptr)
{

    if (thread_number_ <= 0 || (max_requests_ <= 0))
    {
        throw std::exception();
    }
    threads_ = new pthread_t[thread_number_];
    if (!threads_)
    {
        throw std::exception();
    }

    // 创建thread_number个线程并将它们设置为线程脱离
    for (int i = 0; i < thread_number_; i++)
    {
        printf("create the %dth thread\n", i);
        if (pthread_create(threads_ + i, nullptr, Worker, this) != 0)
        {
            delete[] threads_;
            throw std::exception();
        }
        // 线程分离
        if (pthread_detach(threads_[i]))
        {
            delete[] threads_;
            throw std::exception();
        }
    }
}
template <typename T>
ThreadPool<T>::~ThreadPool()
{
    delete[] threads_;
    stop_ = true;
}

template <typename T>
bool ThreadPool<T>::Append(T *request)
{
    queuelocker_.Lock();
    // 如果超出最大的等待处理的任务数量
    if (workqueue_.size() > max_requests_)
    {
        queuelocker_.Unlock();
        return false;
    }

    workqueue_.push_back(request);
    queuelocker_.Unlock();
    // 信号量增加
    queuestat_.Post();
    return true;
}

template <typename T>
void *ThreadPool<T>::Worker(void *arg)
{
    ThreadPool *pool = (ThreadPool *)arg;
    pool->Run();
    return pool;
}

template <typename T>
void ThreadPool<T>::Run()
{
    while (!stop_)
    {
        // 信号量
        queuestat_.Wait();
        queuelocker_.Lock(); // 上锁
        if (workqueue_.empty())
        {
            queuelocker_.Unlock(); // 解锁
            continue;
        }

        T *request = workqueue_.front();
        workqueue_.pop_front();
        queuelocker_.Unlock();

        // 没有获取到
        if (!request)
        {
            continue;
        }
        request->Process();
    }
}
#endif