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
using namespace mywebserver;
// signal capture
void addsig(int sig, void(handler)(int)){
    struct sigaction sa;
    memset(&sa,'\0',sizeof(sa));
    sa.sa_handler=handler;
    sigfillset(&sa.sa_mask);
    sigaction(sig,&sa,NULL);
}
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

    return 0;
}