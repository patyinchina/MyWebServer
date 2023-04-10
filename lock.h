#ifndef LOCK_H_
#define LOCK_H_

#include<pthread.h>
#include<exception>
#include<time.h>
#include<semaphore.h>
namespace mywebserver{

    // 互斥锁类
    class locker{
    public:
        locker(){
            if(pthread_mutex_init(&m_mutex,NULL)!=0){ // 0:error
                throw std::exception(); // 抛出异常
            }
        }
        ~locker(){
            pthread_mutex_destroy(&m_mutex);
        }
        bool lock(){
            return pthread_mutex_lock(&m_mutex)==0;
        }
        bool unlock(){
            return pthread_mutex_unlock(&m_mutex)==0;
        }
        pthread_mutex_t *get_locker(){
            return &m_mutex;
        }
    private:
        pthread_mutex_t m_mutex;
    };// lock

    // 条件变量类
    class cond{
        public:
            cond(){
                if(pthread_cond_init(&m_cond,NULL)!=0){
                    throw std::exception(); // 抛出异常
                }
            }
            ~cond(){
                pthread_cond_destroy(&m_cond);
            }
            bool wait(pthread_mutex_t *mutex){ // 配合互斥锁使用
                return pthread_cond_wait(&m_cond,mutex)==0;
            }
            bool timedwait(pthread_mutex_t *mutex, struct timespec t){ 
                return pthread_cond_timedwait(&m_cond,mutex,&t)==0;
            }
            bool signal(pthread_mutex_t *mutex){ 
                return pthread_cond_signal(&m_cond)==0;
            }
            bool broadcast(){ 
                return pthread_cond_broadcast(&m_cond)==0;
            }
        private:
        pthread_cond_t m_cond;
    };// cond

    // 信号量类
    class sem{
        public:
        sem(){
            if(sem_init(&m_sem,0,0)!=0){
                throw std::exception();
            }
        }
        sem(int num){
            if(sem_init(&m_sem,0,num)!=0){
                throw std::exception();
            }
        }
        ~sem(){
            sem_destroy(&m_sem);
        }
        bool wait(){
            return sem_wait(&m_sem)==0;
        }
        bool post(){
            return sem_post(&m_sem)==0;
        }
        private:
        sem_t m_sem;
    }; // sem

} // mywebserver

#endif // LOCK_H_
