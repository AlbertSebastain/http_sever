# ifndef THREAD_POOL_H
# define THREAD_POOL_H
# include <stack>
# include <cstdio>
# include <mutex>
# include <thread>
# include <exception>
# include <iostream>
# include "locker.h"
# include "logging.h"
# include "util.h"
using namespace std;
template<typename T> class threadpool;
template<typename T> 
class threadpool
{
    public:
        threadpool(config_t config);
        ~threadpool();
        bool append(T* request);
        void run();
    private:
        int m_thread_num;
        int m_max_requests;
        thread** m_threads;
        stack<T*> m_workstack;
        mutex m_mutex;
        sem m_queuestat;
        bool m_stop;
};
template<typename T>
threadpool<T>::threadpool(config_t config)
{
    m_thread_num = config.get_thread_num();
    m_max_requests = config.get_thread_num();
    m_stop = false;
    m_threads = nullptr;
    debug("m thread_num is %d", m_thread_num);
    debug("m max_requests is %d", m_max_requests);
    if((m_thread_num <= 0) || (m_max_requests <= 0))
    {
        log_err("thread num or  max request smaller than zero");
        exit(1);
    }
    m_threads = new thread*[m_thread_num];
    for(int  i=0;i<m_thread_num;i++)
    {
        debug("generate thread %d", i);
        m_threads[i] = new thread(&threadpool::run, this);
        if(m_threads[i]->joinable())
        {
            m_threads[i]->detach();
        }
    }
}
template<typename T>
threadpool<T>::~threadpool()
{
    for(int i=0;i<m_thread_num;i++)
    {
        delete m_threads[i];
    }
    delete[] m_threads;
    m_stop = true;
}
template<typename T>
bool threadpool<T>::append(T* request)
{
    m_mutex.lock();
    int size;
    size = m_workstack.size();
    if(size >= m_max_requests)
    {
        m_mutex.unlock();
        return false;
    }
    m_workstack.push(request);
    m_mutex.unlock();
    m_queuestat.post();
    return true;
}
template<typename T>
void threadpool<T>::run()
{
    while(! m_stop)
    {
        m_queuestat.wait();
        T* request = nullptr;
        m_mutex.lock();
        if(! m_workstack.empty())
        {
            request = m_workstack.top();
            m_workstack.pop();
            m_mutex.unlock();
            if(request == nullptr)
            {
                continue;
            }
                request->process();
        }
        else
        {
            m_mutex.unlock();
        }
    }
}
# endif