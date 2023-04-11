#ifndef THREADPOOL_H_
#define THREADPOOL_H_

#include <pthread.h>
#include <exception>
#include <list>
#include <iostream>
#include "lock.h"

namespace mywebserver{

    // 线程池类
    template<class T>
    class threadpool{
        public:
            threadpool(int thread_number=8, int max_requests=50000); // default
            ~threadpool();
            bool add_request(T* request);
            
        private:
            static void* worker(void* arg);
            void run();
        private:
            int m_thread_number; // number of threads
            pthread_t* m_threads; // array of threadpool
            int m_max_requests; // maximum of requests
            std::list<T*> m_workqueue; // request queue
            locker m_queuelock; // mutex 
            sem m_queuestat; // semaphore
            bool m_stop; // stop current thread
    };
    // threadpool
    template<class T>
    threadpool<T>::threadpool(int thread_number,int max_requests){
        m_thread_number=thread_number;
        m_max_requests=max_requests;
        m_threads=NULL; m_stop=false;
        if(thread_number<=0||max_requests<=0){
            throw std::exception();
        }
        m_threads=new pthread_t[m_thread_number];
        if(!m_threads){
            throw std::exception();
        }
        // create threads (detach)
        for(int i=0;i<thread_number;i++){
            std::cout<<"Creating no."<<i<<" thread"<<std::endl;
            if(pthread_create(m_threads+i,NULL,worker,this)!=0){
                delete [] m_threads;
                throw std::exception();
            }  // worker must be a static funtion !!
            if(pthread_detach(m_threads[i])!=0){
                delete [] m_threads;
                throw std::exception();
            }
        }

    }
    // ~threadpool
    template<class T>
    threadpool<T>::~threadpool(){
        delete [] m_threads;
        m_stop=true;
    }
    // add_request
    template<class T>
    bool threadpool<T>::add_request(T* request){
        m_queuelock.lock();
        if(m_workqueue.size()>m_max_requests){
            m_queuelock.unlock();
            return false;
        }
        m_workqueue.push_back(request);
        m_queuelock.unlock();
        m_queuestat.post();
        return true;
    }
    // worker
    template<class T>
    void* threadpool<T>::worker(void* arg){
        threadpool* pool=(threadpool*)arg;
        pool->run();
        return pool;
    }
    // run
    template<class T>
    void threadpool<T>::run(){
        while(!m_stop){
            m_queuestat.wait();
            m_queuelock.lock();
            if(m_workqueue.empty()){
                m_queuelock.unlock();
                continue;
            }
            T* request=m_workqueue.front();
            m_workqueue.pop_front();
            m_queuelock.unlock();
            if(!request) continue;
            request->process(); // process request
        }

    }

} // mywebserver

#endif // THREADPOOL_H_