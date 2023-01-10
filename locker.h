#pragma once

// 线程同步机制封装类

#include <exception>
#include <mutex>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

// 互斥锁类
class Locker
{
public:
    Locker()
    {
        if (pthread_mutex_init(&mutex_, nullptr) != 0)
        {
            throw std::exception();
        }
    }

    ~Locker() { pthread_mutex_destroy(&mutex_); }

    // 上锁
    bool Lock() { return pthread_mutex_lock(&mutex_) == 0; }
    // 解锁
    bool Unlock() { return pthread_mutex_unlock(&mutex_) == 0; }

    pthread_mutex_t *Get() { return &mutex_; }

private:
    pthread_mutex_t mutex_;
};
// 条件变量类
class Cond
{
public:
    Cond()
    {
        if (pthread_cond_init(&cond_, nullptr) != 0)
        {
            throw std::exception();
        }
    }
    ~Cond() { pthread_cond_destroy(&cond_); }
    bool Wait(pthread_mutex_t *mutex)
    {
        return pthread_cond_wait(&cond_, mutex) == 0;
    }

    bool TimeWait(pthread_mutex_t *mutex, struct timespec t)
    {
        return pthread_cond_timedwait(&cond_, mutex, &t) == 0;
    }
    bool Signal() { return pthread_cond_signal(&cond_) == 0; }
    bool Broadcast() { return pthread_cond_broadcast(&cond_) == 0; }

private:
    pthread_cond_t cond_;
};

// 信号量类
class Sem
{
public:
    Sem()
    {
        if (sem_init(&sem_, 0, 0) != 0)
        {
            throw std::exception();
        }
    }
    ~Sem() { sem_destroy(&sem_); }
    // 等待信号量
    bool Wait() { return sem_wait(&sem_) == 0; }
    // 增加信号量
    bool Post() { return sem_post(&sem_) == 0; }

private:
    sem_t sem_;
};
