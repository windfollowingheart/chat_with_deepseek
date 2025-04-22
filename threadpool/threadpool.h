#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <iostream>
#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"
#include "../CGIredis/redis_connection_pool.h"
#include "../CGIrabbitmq/rabbitmq_connection_pool.h"
#include "../CGIuploadfile/uploadfile_connection_pool.h"
#include "../CGIgetparseresult/getparseresult_connection_pool.h"
#include "../token/tokenpool.h"

template <typename T>
class threadpool
{
public:
    /*thread_number是线程池中线程的数量，max_requests是请求队列中最多允许的、等待处理的请求的数量*/
    threadpool(connection_pool *connPool, 
        redis_connection_pool *reids_connPool, 
        rabbitmq_connection_pool *rabbitmq_connPool, 
        uploadfile_connection_pool *uploadfile_connPool,
        getparseresult_connection_pool *getparseresult_connPool,
        token_pool *tokenPool, 
        int thread_number = 1000, int max_request = 10000);
    ~threadpool();
    bool append(T *request);


private:
    /*工作线程运行的函数，它不断从工作队列中取出任务并执行之*/
    static void *worker(void *arg);
    void run();

private:
    int m_thread_number;        //线程池中的线程数
    int m_max_requests;         //请求队列中允许的最大请求数
    pthread_t *m_threads;       //描述线程池的数组，其大小为m_thread_number
    std::list<T *> m_workqueue; //请求队列
    locker m_queuelocker;       //保护请求队列的互斥锁
    sem m_queuestat;            //是否有任务需要处理
    bool m_stop;                //是否结束线程
    connection_pool *m_connPool;  //数据库
    redis_connection_pool *m_redis_connPool;  //redis
    rabbitmq_connection_pool *m_rabbitmq_connPool;  //rabbitmq
    uploadfile_connection_pool *m_uploadfile_connPool;  //python后端上传
    getparseresult_connection_pool *m_getparseresult_connPool;  //python后端获取解析结果
    token_pool *m_token_pool;  //token
};
template <typename T>
threadpool<T>::threadpool( connection_pool *connPool, 
    redis_connection_pool *reids_connPool, 
    rabbitmq_connection_pool *rabbitmq_connPool,
    uploadfile_connection_pool *uploadfile_connPool,
    getparseresult_connection_pool *getparseresult_connPool,
    token_pool *tokenPool, 
    int thread_number, int max_requests)
: m_thread_number(thread_number), 
m_max_requests(max_requests), 
m_stop(false), 
m_threads(NULL),
m_connPool(connPool),
m_redis_connPool(reids_connPool),
m_rabbitmq_connPool(rabbitmq_connPool),
m_uploadfile_connPool(uploadfile_connPool),
m_getparseresult_connPool(getparseresult_connPool),
m_token_pool(tokenPool)
{
    // std::cout << "hello" << std::endl;
    if (thread_number <= 0 || max_requests <= 0)
        throw std::exception();
    m_threads = new pthread_t[m_thread_number];
    if (!m_threads)
        throw std::exception();
    for (int i = 0; i < thread_number; ++i)
    {
        //printf("create the %dth thread\n",i);
        if (pthread_create(m_threads + i, NULL, worker, this) != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }
        if (pthread_detach(m_threads[i]))
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
    // std::cout << "hello1" << std::endl;
}
template <typename T>
threadpool<T>::~threadpool()
{
    delete[] m_threads;
    m_stop = true;
}
template <typename T>
bool threadpool<T>::append(T *request)
{
    m_queuelocker.lock();
    if (m_workqueue.size() > m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}
template <typename T>
void *threadpool<T>::worker(void *arg) // 一个worker对应一个工作线程, 
{
    threadpool *pool = (threadpool *)arg;
    pool->run();
    return pool;
}
template <typename T>
void threadpool<T>::run() // 在一个工作线程中会轮询任务队列，即list, 加锁确保线程安全
{
    while (!m_stop)
    {
        m_queuestat.wait();
        m_queuelocker.lock();
        if (m_workqueue.empty())
        {
            m_queuelocker.unlock();
            continue;
        }
        T *request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if (!request)
            continue;

        // connectionRAII mysqlcon(&request->mysql, m_connPool);
        // redis_connectionRAII redislcon(&request->redis, m_redis_connPool);
        // rabbitmq_connectionRAII rabbitmqlcon(&request->rabbitmq, m_rabbitmq_connPool);
        request->m_token_pool = m_token_pool;
        request->m_mysql_pool = m_connPool;
        request->m_redis_pool = m_redis_connPool;
        request->m_rabbitmq_pool = m_rabbitmq_connPool;
        request->m_uploadfileconn_pool = m_uploadfile_connPool;
        request->m_getparseresult_pool = m_getparseresult_connPool;
        
        request->process();
    }
}
#endif
