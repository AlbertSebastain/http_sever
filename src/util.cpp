# include "util.h"
using namespace std;
int setnonblock(int conn_fd)
{
    int old_option;
    old_option = fcntl(conn_fd, F_GETFL, 0);
    int new_option;
    new_option = old_option | O_NONBLOCK;
    fcntl(conn_fd, F_SETFL, new_option);
    return old_option;
}
void addfd(int epoll_fd, int conn_fd, bool one_shot)
{
    epoll_event event;
    event.data.fd = conn_fd;
    event.events = EPOLLIN| EPOLLET | EPOLLRDHUP;
    if(one_shot)
    {
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_fd, &event);
    setnonblock(conn_fd);
}
void modfd(int epoll_fd, int conn_fd, int events)
{
    epoll_event event;
    event.data.fd = conn_fd;
    event.events = events | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, conn_fd, &event);
}
void deletefd(int epoll_fd, int conn_fd)
{
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, conn_fd, 0);
    close(conn_fd);
}
config_t::config_t(string const config_file): file_path(config_file), thread_num(8), max_request(1000)
{
    parse_config_file();
}
config_t::config_t(): file_path(""), thread_num(8), max_request(1000)
{}
bool config_t::parse_config_file()
{
    ifstream in(file_path.c_str());
    if(! in.is_open())
    {
        log_err("the config file:%s can not be opend",file_path.c_str());
        exit(1);
    }
    while(!in.eof())
    {
        char buffer[1024];
        bzero(buffer, sizeof(buffer));
        int i=0;
        string temp;
        in.getline(buffer,100);
         buffer;
       char* tar;
        tar = strchr(buffer, '=');
        *tar = '\0';
        tar++;
        if(strcmp(buffer, "thread_num"))
        {
            thread_num = atoi(tar);
        }
        else if(strcmp(buffer, "max_requests"))
        {
            max_request = atoi(tar);
        }
    }
    return;
}
int config_t::get_thread_num() const
{
    return thread_num;
}
int config_t::get_max_request() const
{
    return max_request;
}