#ifndef LST_TIMER_H_
#define LST_TIMER_H_

#include <stdio.h>
#include <time.h>
#include <arpa/inet.h>

class util_timer; // 前置声明

struct client_data{
    sockaddr_in address;
    int sockfd;
    util_timer* timer;
};

class util_timer{
public:
    util_timer():prev(NULL),next(NULL){}
public:
    time_t expire;
    void (*cb_func)(client_data); // callback function
    client_data* user_data;
    util_timer* prev;
    util_timer* next;
};

// 定时器链表，升序双向链表
class timer_list{
public:
    timer_list(){}
    ~timer_list(){

    }
    void add_timer(util_timer* timer){

    }
    void adjust_timer(util_timer* timer){

    }
    void del_timer(util_timer* timer){

    }
    void tick(){

    }
private:
    util_timer* head;
    util_timer* tail;
};

#endif //LST_TIMER_H_