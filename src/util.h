# ifndef UTIL_H
# define UTIL_H
# include <cstdio>
# include <string>
# include <cstring>
# include <iostream>
# include <fstream>
# include <fcntl.h>
# include <sys/socket.h>
# include <sys/types.h>
# include <signal.h>
# include <sys/stat.h>
# include <unistd.h>
# include <sys/epoll.h>
# include "logging.h"

# define BUFFER_SIZE 1024
# define CR  '\r'
# define LF '\n'

using namespace std;
int setnonblock(int);
void addfd(int, int, bool);
void modfd(int, int, int);
void deletefd(int, int);
void addsig(int sig,void(handler)(int),bool restart = true); 
void show_error(int connfd, const char* info);
class config_t;
class config_t
{
    public:
        config_t(std::string const);
        config_t();
        ~config_t(){}
        bool parse_config_file();
        int get_thread_num() const;
        int get_max_request() const;
        int get_port() const;
    private:
        std::string file_path;
        int thread_num;
        int max_request;
        int port;
};
# endif