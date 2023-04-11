#ifndef HTTP_CONN_H_
#define HTTP_CONN_H_

#include <iostream>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

namespace mywebserver{
    class http_conn{
    public:
        static int m_epollfd; // all socket events
        static int m_user_count; // user number

        http_conn(){}
        ~http_conn(){}

        void process(); // process requests from clients
        // initialize new connection
        void init(int sockfd, const struct sockaddr_in &addr);
        // close connection
        void close_conn();
        // read (nonblocking)
        bool read();
        // write (nonblocking)
        bool write();
    private:
        int m_sockfd; // conneted socket
        sockaddr_in m_address; // socket address

    };
} // mywebserver

#endif // HTTP_CONN_H_