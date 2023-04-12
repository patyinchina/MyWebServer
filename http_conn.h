#ifndef HTTP_CONN_H_
#define HTTP_CONN_H_

#include <iostream>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

namespace mywebserver{
    class http_conn{
    public:
        static int m_epollfd; // all socket events
        static int m_user_count; // user number
        static const int FILENAME_LEN=200;
        static const int read_buf_size=4096;
        static const int write_buf_size=4096;
        enum METHOD { 
            GET=0, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT, PATH 
        };
        // 主状态机状态
        enum CHECK_STATE { 
            CHECK_STATE_REQUESTLINE=0, CHECK_STATE_HEADER, CHECK_STATE_CONTENT
        };
        // 从状态机状态
        enum LINE_STATUS {
            LINE_OK=0, LINE_BAD, LINE_OPEN
        };
        // 服务器处理http请求的可能结果，报文解析结果
        enum HTTP_CODE {
            NO_REQUEST=0, GET_REQUEST, BAD_REQUEST, NO_RESOURCE, FORBIDDEN_REQUEST, FILE_REQUEST, INTERNAL_ERROR, CLOSED_CONNECTION
        };
        // content type
        enum CONTENT_TYPE{
            TEXT=0, ICON
        };
    
    public:
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
        void init(); // initialize other information
        HTTP_CODE process_read(); // parse http request
        HTTP_CODE parse_request_line(char* text); // parse first line
        HTTP_CODE parse_request_headers(char* text); // parse headers
        HTTP_CODE parse_request_contents(char* text); // parse contents
        LINE_STATUS parse_oneline();
        char* get_line() {return m_read_buf+m_start_line;}
        HTTP_CODE do_request();
        void unmap();

        bool process_write(HTTP_CODE read_ret);
        bool add_status_line(int status,const char* title);
        bool add_response(const char* format, ...);
        bool add_headers(const int content_len);
        bool add_content_length(const int content_len);
        bool add_content_type();
        bool add_linger();
        bool add_blank_line();
        bool add_content(const char* content);

    private:
        int m_sockfd; // conneted socket
        sockaddr_in m_address; // socket address

        char m_read_buf[read_buf_size]; // read buffer
        int m_read_index; // next position
        int m_checked_index; // current parsing index in read buffer
        int m_start_line; // current parsing line

        char m_write_buf[write_buf_size]; // write buffer
        int m_write_index; 
        char m_real_file[500];
        struct stat m_file_stat;
        char* m_file_addr;
        struct iovec m_iv[2];
        int m_iv_count;
        int bytes_sent;
        int bytes_to_be_sent;
        
        char* m_url; // url
        char* m_version; // version of http
        METHOD m_method; // request method
        char* m_host; // host
        bool m_linger; // keep_alive or not

        int m_content_length; // length of contents
        CONTENT_TYPE m_content_type; // type of contents

        CHECK_STATE m_check_stat; // 主状态机状态

    private:
        
    };
} // mywebserver

#endif // HTTP_CONN_H_