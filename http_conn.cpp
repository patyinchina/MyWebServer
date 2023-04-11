#include"http_conn.h"

// oneshot: 一个socket连接在任一时刻只能被一个线程处理

// set nonblocking
int setnonblocking(int fd){
    int old_flag=fcntl(fd,F_GETFL);
    int new_flag=old_flag|O_NONBLOCK;
    fcntl(fd,F_SETFL,new_flag);
}

// add fd into epoll
void addfd(int epollfd,int fd,bool one_shot){
    epoll_event event;
    event.data.fd=fd;
    // event.events=EPOLLIN|EPOLLET; // ET mode
    event.events=EPOLLIN | EPOLLRDHUP; // LT mode
    // EPOLLRDHUP: 异常断开时无需交给上层处理
    if(one_shot){
        event.events|=EPOLLONESHOT;
    }
    // set nonblocking
    setnonblocking(fd);
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
}

// remove fd from epoll
void removefd(int epollfd,int fd){
    epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,0);
    close(fd);
}

// modify fd (reset EPOLLONESHOT)
void modfd(int epollfd,int fd,int ev){
    epoll_event event;
    event.data.fd=fd;
    event.events=ev|EPOLLONESHOT|EPOLLRDHUP;
    epoll_ctl(epollfd,EPOLL_CTL_MOD,fd,&event);
}

namespace mywebserver{

int http_conn::m_epollfd=-1; // all socket events
int http_conn::m_user_count=0; // user number

// initialize new connection
void http_conn::init(int sockfd, const struct sockaddr_in &addr){
    m_sockfd=sockfd;
    m_address=addr;
    // reusable port 
    int reuse=1;
    setsockopt(m_sockfd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
    addfd(m_epollfd,sockfd,true);
    m_user_count++;
}

// close connection
void http_conn::close_conn(){
    if(m_sockfd!=-1){
        removefd(m_epollfd,m_sockfd);
        m_sockfd=-1;
        m_user_count--;
    }
}

// read (nonblocking)
bool http_conn::read(){
    std::cout<<"read all data"<<std::endl;

    return true;
}

// write (nonblocking)
bool http_conn::write(){
    std::cout<<"write all data"<<std::endl;

    return true;
}

// process http requests from clients
void http_conn::process(){
    // parse http request
    std::cout<<"parse request, create response"<<std::endl;

    // respond http request
    std::cout<<"respond request"<<std::endl;
}

} //mywebserver