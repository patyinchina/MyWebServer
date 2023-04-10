#ifndef HTTP_CONN_H_
#define HTTP_CONN_H_

namespace mywebserver{
    class http_conn{
    public:
        http_conn();
        ~http_conn();
        void process(); // process requests from clients
    private:

    };
    http_conn::http_conn(){

    }
    http_conn::~http_conn(){

    }
} // mywebserver

#endif // HTTP_CONN_H_