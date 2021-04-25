# ifndef HTTP_CONN_H
# define HTTP_CONN_H
# include <functional>
# include <unistd.h>
# include <signal.h>
# include <sys/types.h>
# include <sys/epoll.h>
# include <fcntl.h>
# include <sys/socket.h>
# include <sys/mman.h>
# include <string.h>
# include <thread>
# include <stdarg.h>
# include <errno.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <assert.h>
# include <time.h>
# include <cstdio>
# include <iostream>
# include <vector>
# include <map>
# include <sys/stat.h>
# include <sys/uio.h>
# include "locker.h"
# include "logging.h"
# include "util.h"

using namespace std;
class http_conn;
class heap_timer;
class heap_timer
{
    public:
        heap_timer(int, http_conn*);
        ~heap_timer(){}
        time_t expire;
        http_conn* http_instance;
        bool stop_timer;
};
struct header_fuc
{
    string header;
    function<void(char*)> fuc;
};
class http_conn
{
    public:
        static const int DEFAULT_TIME = 500;
        static const int FILENAME_LEN = 200;
        static const int READ_BUFFER_SIZE = BUFFER_SIZE;
        static const int WRITE_BUFFER_SIZE = BUFFER_SIZE;
        enum METHOD{GET, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT, PATCH};
        enum CHECK_STATE{CHECK_STATE_REQUESTLINE, CHECK_STATE_HEADER, CHECK_STATE_CONTENT};
        enum HTTP_CODE{NO_REQUEST, GET_REQUEST, BAD_REQUEST, NO_RESOURCE, FORBIDDEN_REQUEST,FILE_REQUEST, INTERNAL_ERROR, CLOSE_CONNECTION}; 
        enum LINE_STATUS{LINE_OK, LINE_BAD, LINE_OPEN};
        http_conn(){}
        ~http_conn();
        void init(int, const sockaddr_in&);
        void close_conn(bool close_conn_able = true);
        void process();
        bool read();
        bool write();
        void set_timer_handler(function<void()>);
        void header_process_host(char*);
        void header_process_connection(char*);
        void header_process_content_length(char*);
    private:
        void init();
        HTTP_CODE process_read();
        bool process_write(HTTP_CODE);
        HTTP_CODE parse_request_line(char*);
        HTTP_CODE parse_header(char*);
        HTTP_CODE parse_content(char*);
        HTTP_CODE do_request();
        char* get_line() {return m_read_buf+m_start_line;}
        LINE_STATUS parse_line();
        void init_header_map();
        void unmap();
        bool add_response(const char* format, ...);
        bool add_content(const char*);
        bool add_status_line(int, const char*);
        bool add_headers(int,int);
        bool add_content_length(int);
        bool add_linger();
        bool add_blank_line();
        bool add_host_name();
        bool add_content_type();
        bool add_last_modified();
        string uniform_content(const char*);
    public:
        static int m_epollfd;
        static int m_user_count;
         function<void()> timer_handler;
         vector<heap_timer*> timers_for_http;
    private:
        int m_sockfd;
        sockaddr_in m_address;
        char m_read_buf[READ_BUFFER_SIZE];
        int m_read_idx;
        int m_checked_idx;
        int m_start_line;
        char m_write_buf[WRITE_BUFFER_SIZE];
        int m_write_idx;
        header_fuc m_header_map[3];
        CHECK_STATE m_check_state;
        METHOD m_method;
        char m_real_file[FILENAME_LEN];
        char m_read_content[8096];
        char* m_url;
        char* m_version;
        char* m_host;
        int m_content_length;
        bool m_linger;
        char* m_file_address;
        struct stat m_file_stat;
        struct iovec m_iv[2];
        int m_iv_count;
        char* m_file_type;
        time_t m_file_time;
        
};
#endif