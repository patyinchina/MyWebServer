#include <iostream>
#include <stdlib.h>
#include <cstring>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include "lock.h"
#include "threadpool.h"
#include "http_conn.h"
#define MAX_FD 65535 
#define MAX_EVENT_NUM 50000 

using namespace mywebserver;

// signal capture
void addsig(int sig, void(handler)(int)){
    struct sigaction sa;
    memset(&sa,'\0',sizeof(sa));
    sa.sa_handler=handler;
    sigfillset(&sa.sa_mask);
    sigaction(sig,&sa,NULL);
}

// add fd into epoll
extern void addfd(int epollfd,int fd,bool one_shot,bool ET_mode);
// remove fd from epoll
extern void removefd(int epollfd,int fd);
// modify fd
extern void modfd(int epollfd,int fd,int ev);

int main(int argc, char* argv[]){

    if(argc<=1){
        std::cout<<"Please use this format: "<<basename(argv[0])<<" port_number"<<std::endl;
    }
    // port number
    int port=atoi(argv[1]);
    // SIGPIPE
    addsig(SIGPIPE,SIG_IGN);
    // initialize threadpool
    threadpool<http_conn>* pool=NULL;
    try{
        pool=new threadpool<http_conn>;
    }
    catch(...){
        exit(-1);
    }

    // create an array to save information of clients
    http_conn* users=new http_conn[MAX_FD];

    // socket
    int listenfd=socket(AF_INET,SOCK_STREAM,0);

    // reusable port 
    int reuse=1;
    setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
    struct sockaddr_in saddr;
    saddr.sin_family=AF_INET;
    saddr.sin_port=htons(port);
    // inet_pton(AF_INET,"127.0.0.1",&saddr.sin_addr.s_addr);
    saddr.sin_addr.s_addr=INADDR_ANY;

    bind(listenfd,(struct sockaddr*)&saddr,sizeof(saddr));
    listen(listenfd,10);

    // epoll
    epoll_event events[MAX_EVENT_NUM];
    int epollfd=epoll_create(10);

    addfd(epollfd,listenfd,false,false);
    http_conn::m_epollfd=epollfd;
    http_conn::m_user_count=0;

    while(1){
        int num=epoll_wait(epollfd,events,MAX_EVENT_NUM,-1);
        if(num<0&&errno!=EINTR){
            std::cout<<"epoll failure"<<std::endl;
            exit(-1);
        }

        for(int i=0;i<num;i++){
            int sockfd=events[i].data.fd;
            if(sockfd==listenfd){
                // a client is connenting
                struct sockaddr_in caddr;
                socklen_t clen=sizeof(caddr);
                int connfd=accept(listenfd,(struct sockaddr*)&caddr,&clen);
                char clientaddr_buf[100];
                inet_ntop(AF_INET,&caddr.sin_addr.s_addr,clientaddr_buf,sizeof(clientaddr_buf));
                std::cout<<"client address : "<<clientaddr_buf<<" client port :"<<ntohs(caddr.sin_port)<<std::endl;
                if(http_conn::m_user_count>=MAX_FD){
                    // server is busy
                    // ... // inform the conneting client
                    close(connfd);
                    continue;
                }
                std::cout<<"connection created"<<std::endl;
                // initialize new connection
                users[connfd].init(connfd,caddr); // 采用文件描述符作为索引
            }
            else if(events[i].events&(EPOLLRDHUP|EPOLLHUP|EPOLLERR)){
                // abnormal event of error
                users[sockfd].close_conn();
            }
            else if(events[i].events&EPOLLIN){
                if(users[sockfd].read()){
                    // read all data
                    pool->add_request(users+sockfd);
                }
                else{
                    std::cout<<"read error"<<std::endl;
                    users[sockfd].close_conn();
                }
            }
            else if(events[i].events&EPOLLOUT){
                // write all data
                if(!users[sockfd].write()){
                    std::cout<<"write error"<<std::endl;
                    users[sockfd].close_conn();
                }
                std::cout<<"write done :)"<<std::endl<<std::endl;
            }
        }
    }
    close(epollfd);
    close(listenfd);
    delete [] users;
    delete pool;

    return 0;
}