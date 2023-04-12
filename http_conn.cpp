#include"http_conn.h"

// oneshot: 一个socket连接在任一时刻只能被一个线程处理

// define response information
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

// web root
const char* doc_root="/home/pat/MyWebServer/resources";

// set nonblocking
int setnonblocking(int fd){
    int old_flag=fcntl(fd,F_GETFL);
    int new_flag=old_flag|O_NONBLOCK;
    fcntl(fd,F_SETFL,new_flag);
}

// add fd into epoll
void addfd(int epollfd,int fd,bool one_shot,bool ET_mode){
    epoll_event event;
    event.data.fd=fd;
    if(ET_mode) event.events=EPOLLIN | EPOLLET | EPOLLRDHUP; // ET mode
    else event.events=EPOLLIN | EPOLLRDHUP; // LT mode
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
    // addfd(m_epollfd,sockfd,true,false); //LT mode
    addfd(m_epollfd,sockfd,true,true); //ET mode
    m_user_count++;
    init();
}

void http_conn::init(){
    m_check_stat=CHECK_STATE_REQUESTLINE; // initial state: parse first line
    m_checked_index=0;
    m_start_line=0;
    m_read_index=0;

    m_write_index = 0;
    bytes_sent=0;
    bytes_to_be_sent=0;

    m_method=GET;
    m_url=0;
    m_version=0;
    m_linger=false;
    
    m_content_length = 0;
    m_content_type=TEXT;

    memset(m_read_buf,0,sizeof(m_read_buf));
    memset(m_real_file,0,sizeof(m_real_file));
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
    if(m_read_index>=read_buf_size){
        return false;
    }
    // read byte
    int byte_read=0;
    while(true){
        byte_read=recv(m_sockfd,m_read_buf+m_read_index,read_buf_size-m_read_index,0);
        if(byte_read==-1){
            if(errno==EAGAIN||errno==EWOULDBLOCK){
                // no data
                break;
            }
            else return false;
        }
        else if(byte_read==0){
            // connection closed
            return false;
        }
        m_read_index+=byte_read;
    }
    std::cout<<"receive data: "<<std::endl<<m_read_buf<<std::endl;
    return true;
}

// write (nonblocking)
bool http_conn::write(){
    // 分散写
    int tmp=0;
    if(bytes_to_be_sent==0){
        modfd(m_epollfd,m_sockfd,EPOLLIN);
        init();
        return true;
    }
    while(1){
        //std::cout<<"writing"<<std::endl;
        //std::cout<<"bytes_to_be_sent : "<<bytes_to_be_sent<<" bytes_sent : "<<bytes_sent<<std::endl;
        tmp=writev(m_sockfd,m_iv,m_iv_count);
        //std::cout<<"write return : "<<tmp<<std::endl;
        //std::cout<<"write buffer : "<<std::endl<<m_write_buf<<std::endl;
        if(tmp<=-1){
            if(errno==EAGAIN){
                modfd(m_epollfd,m_sockfd,EPOLLOUT);
                return true;
            }
            unmap();
            return false;
        }
        bytes_to_be_sent-=tmp;
        bytes_sent+=tmp;
        if(bytes_sent>=m_write_index){
            m_iv[0].iov_len=0;
            m_iv[1].iov_base=m_file_addr+(bytes_sent-m_write_index);
            m_iv[1].iov_len=bytes_to_be_sent;
        }
        else{
            m_iv[0].iov_base=m_write_buf+bytes_sent;
            m_iv[0].iov_len=m_iv[0].iov_len-bytes_sent;
        }
        if(bytes_to_be_sent<=0){
            unmap();
            modfd(m_epollfd,m_sockfd,EPOLLIN);
            if(m_linger){
                init();
                return true;
            }
            else{
                return false;
            }
        }
    }
    return true;
}

// process http requests from clients
void http_conn::process(){
    // parse http request
    std::cout<<"parse request, create response"<<std::endl;
    HTTP_CODE read_ret=process_read();
    if(read_ret==NO_REQUEST){
        modfd(m_epollfd,m_sockfd,EPOLLIN);
        return;
    }

    // respond http request
    bool write_ret=process_write(read_ret);
    if(!write_ret){
        close_conn();
    }
    modfd(m_epollfd,m_sockfd,EPOLLOUT);
}

// parse http request
http_conn::HTTP_CODE http_conn::process_read(){
    LINE_STATUS linestat=LINE_OK;
    HTTP_CODE ret=NO_REQUEST;
    char* text=0;
    while((m_check_stat==CHECK_STATE_CONTENT&&linestat==LINE_OK)||((linestat=parse_oneline())==LINE_OK)){
        // parsed an entire line or request contents
        // get one line
        text=get_line();
        m_start_line=m_checked_index;
        // std::cout<<"got 1 http line : "<<text<<std::endl;
        switch(m_check_stat){ // 主状态机
            case CHECK_STATE_REQUESTLINE:{
                ret=parse_request_line(text);
                if(ret==BAD_REQUEST) return BAD_REQUEST;
                break;
            }
            case CHECK_STATE_HEADER:{
                ret=parse_request_headers(text);
                if(ret==BAD_REQUEST) {
                    return BAD_REQUEST;
                }
                else if(ret==GET_REQUEST){
                    return do_request();
                }
                break;
            }
            case CHECK_STATE_CONTENT:{
                ret=parse_request_contents(text);
                if(ret==GET_REQUEST){
                    return do_request();
                }
                linestat=LINE_OPEN;
                break;
            }
            default:{
                return INTERNAL_ERROR;
            }
        }
    }
    return NO_REQUEST;
}

// respond http request
bool http_conn::process_write(http_conn::HTTP_CODE read_ret){
    switch (read_ret){
        case INTERNAL_ERROR:
            add_status_line(500,error_500_title);
            add_headers(strlen(error_500_form));
            if(!add_content(error_500_form)){
                return false;
            }
            break;
        case BAD_REQUEST:
            add_status_line(400,error_400_title);
            add_headers(strlen(error_400_form));
            if(!add_content(error_400_form)){
                return false;
            }
            break;
        case NO_RESOURCE:
            add_status_line(404,error_404_title);
            add_headers(strlen(error_404_form));
            if(!add_content(error_404_form)){
                return false;
            }
            break;
        case FORBIDDEN_REQUEST:
            add_status_line(403,error_403_title);
            add_headers(strlen(error_403_form));
            if(!add_content(error_403_form)){
                return false;
            }
            break;
        case FILE_REQUEST:
            // std::cout<<"handling file request"<<std::endl;
            add_status_line(200,ok_200_title);
            add_headers(m_file_stat.st_size);
            m_iv[0].iov_base=m_write_buf;
            m_iv[0].iov_len=m_write_index;
            m_iv[1].iov_base=m_file_addr;
            m_iv[1].iov_len=m_file_stat.st_size;
            m_iv_count=2;
            bytes_to_be_sent=m_write_index+m_file_stat.st_size;
            return true;
            break;
        default:
            return false;
            break;
    }
    m_iv[0].iov_base=m_write_buf;
    m_iv[0].iov_len=m_write_index;
    m_iv_count=1;
    bytes_to_be_sent=m_write_index;
    return true;
}

// parse first line to get request method, url, http version
http_conn::HTTP_CODE http_conn::parse_request_line(char* text){
    // e.g. GET / HTTP/1.1
    m_url=strpbrk(text," \t");
    *m_url++='\0';
    // → GET\0/ HTTP/1.1
    char* method=text;
    if(strcasecmp(method,"GET")==0){
        m_method=GET;
    }
    else if(strcasecmp(method,"POST")==0){
        m_method=POST;
    }
    else return BAD_REQUEST;

    // GET\0/ HTTP/1.1
    m_version=strpbrk(m_url," \t");
    if(!m_version){
        return BAD_REQUEST;
    }
    *m_version++='\0';
    // → GET\0/\0HTTP/1.1
    if(strcasecmp(m_version,"HTTP/1.1")!=0){
        return BAD_REQUEST;
    }

    // e.g. http://192.168.1.1:10000/index.html
    if(strncasecmp(m_url,"http://",7)==0){
        m_url+=7; // 192.168.1.1:10000/index.html
        m_url=strchr(m_url, '/'); // /index.html
        if(!m_url||m_url[0]!='/'){
            return BAD_REQUEST;
        }
    }

    if (strncasecmp(m_url,"https://",8)==0){
        m_url+=8;
        m_url=strchr(m_url, '/');
    }
    
    if(strncasecmp(m_url,"/homepage.html",14)==0){
        // visit home page
        m_content_type=TEXT;
    }
    if(strncasecmp(m_url,"/favicon.ico",12)==0){
        // favicon.ico
        m_content_type=ICON;
    }

    std::cout<<"request line OK, url : "<<m_url<<std::endl;
    m_check_stat=CHECK_STATE_HEADER; // change state
    return NO_REQUEST;
}

// parse headers
http_conn::HTTP_CODE http_conn::parse_request_headers(char* text){
    //std::cout<<"handling text : "<<text<<std::endl;
    if(text[0]=='\0'){
        if(m_content_length!=0){
            m_check_stat=CHECK_STATE_CONTENT;
            std::cout<<"request headers OK"<<std::endl;
            return NO_REQUEST;
        }
        // get full request
        std::cout<<"request headers OK"<<std::endl;
        return GET_REQUEST;
    }
    else if(strncasecmp(text,"Connection:",11)==0){
        text+=11;
        text+=strspn(text," \t");
        if(strcasecmp(text,"keep-alive")==0){
            m_linger=true;
        }
    }
    else if(strncasecmp(text,"Content-Length:",15)==0){
        text+=15;
        text+=strspn(text," \t");
        m_content_length=atol(text);
    }
    else if(strncasecmp(text,"Host:",5)==0){
        text+=5;
        text+=strspn(text," \t");
        m_host=text;
    }
    else {
        std::cout<<"unknown header... : "<<std::endl<<text<<std::endl;
    }
    return NO_REQUEST;
} 

// parse contents
http_conn::HTTP_CODE http_conn::parse_request_contents(char* text){
    if(m_read_index>=(m_content_length+m_checked_index)){
        text[m_content_length]='\0';
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

// parse one line
http_conn::LINE_STATUS http_conn::parse_oneline(){
    char tmp;
    while(m_checked_index<m_read_index){
        tmp=m_read_buf[m_checked_index];
        if(tmp=='\r'){
            if(m_checked_index+1==m_read_index){
                return LINE_OPEN;
            }
            else if(m_read_buf[m_checked_index+1]=='\n'){
                m_read_buf[m_checked_index++]='\0';
                m_read_buf[m_checked_index++]='\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if(tmp=='\n'){
            if(m_checked_index>1&&m_read_buf[m_checked_index-1]=='\r'){
                m_read_buf[m_checked_index-1]='\0';
                m_read_buf[m_checked_index++]='\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        m_checked_index++;
    }
    return LINE_OPEN;
}

http_conn::HTTP_CODE http_conn::do_request(){
    std::cout<<"handling request"<<std::endl;
    // resources: /home/pat/MyWebServer/resources
    strcpy(m_real_file,doc_root);
    int len=strlen(doc_root);
    strncpy(m_real_file+len, m_url, FILENAME_LEN-len-1);
    if(stat(m_real_file,&m_file_stat)<0){
        std::cout<<"NO_RESOURCE"<<std::endl;
        return NO_RESOURCE;
    }
    // privilege
    if(!(m_file_stat.st_mode&S_IROTH)){
        std::cout<<"FORBIDDEN_REQUEST"<<std::endl;
        return FORBIDDEN_REQUEST;
    }
    // directory
    if(S_ISDIR(m_file_stat.st_mode)){
        std::cout<<"BAD_REQUEST"<<std::endl;
        return BAD_REQUEST;
    }

    // open file
    int fd=open(m_real_file,O_RDONLY);
    m_file_addr=(char*)mmap(0,m_file_stat.st_size,PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    std::cout<<"get file request"<<std::endl;
    return FILE_REQUEST;
}

void http_conn::unmap(){
    if(m_file_addr){
        munmap(m_file_addr,m_file_stat.st_size);
        m_file_addr=0;
    }
}

// write things into write buffer according to a certain format
bool http_conn::add_response(const char* format, ...){
    if(m_write_index>=write_buf_size) return false;
    va_list arg_list;
    va_start(arg_list,format);
    int len=vsnprintf(m_write_buf+m_write_index,write_buf_size-1-m_write_index,format,arg_list);
    if(len>=(write_buf_size-1-m_write_index)) return false;
    m_write_index+=len;
    va_end(arg_list);
    return true;
}

bool http_conn::add_status_line(int status, const char* title){
    return add_response("%s %d %s\r\n","HTTP/1.1",status,title);
}

bool http_conn::add_headers(const int content_len){
    return add_content_type() && add_content_length(content_len) && add_linger() && add_blank_line() ;
}

bool http_conn::add_content_length(const int content_len){
    return add_response("Content-Length:%d\r\n",content_len);
}
bool http_conn::add_content_type(){
    const char* content_type=0;
    switch(m_content_type){
        case TEXT:
            content_type="text/html";
            break;
        case ICON:
            content_type="image/x-icon";
            break;
        default:
            content_type="text/html";
            break;
    }
    return add_response("Content-Type:%s\r\n",content_type);
}
bool http_conn::add_linger(){
    if(m_linger==true)
        return add_response("Connection:%s\r\n","keep-alive");
    else
        return add_response("Connection:%s\r\n","close");
}
bool http_conn::add_blank_line(){
    return add_response("%s","\r\n");
}
bool http_conn::add_content(const char* content){
    //return add_response("%s", content);
}

} //mywebserver