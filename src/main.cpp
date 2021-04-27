# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <cstdio>
# include <unistd.h>
# include <errno.h>
# include <string.h>
# include <fcntl.h>
# include <stdlib.h>
# include <cassert>
# include <sys/epoll.h>
# include "logging.h"
# include "locker.h"
# include "thread_pool.h"
# include "http_conn.h"
# include "util.h"
# include "timer.h"

# define MAX_FD 1000
# define CONFIG_FILE_ROOT "../config.yaml"
# define TIME_SLOT 5

static int pipefd[2];
extern time_heap timer_http;
void sig_handler(int sig)
{
    int save_errno;
    save_errno = errno;
    int msg;
    msg = sig;
    send(pipefd[1], (char*)&msg, 1,0);
    errno = save_errno;
}
void timer_handler()
{
    timer_http.tick();
    alarm(TIME_SLOT);
}
int main()
{
    addsig(SIGALRM, sig_handler);
    addsig(SIGTERM, sig_handler);
    addsig(SIGPIPE, SIG_IGN);
    int ret = 0;
    ret = socketpair(AF_UNIX,SOCK_STREAM, 0, pipefd);
    check(ret >= 0, "sock pair fail");
    string config_file;
    config_file = CONFIG_FILE_ROOT;
    config_t args(config_file);
    bool timeout = false;
    threadpool<http_conn>* pool = nullptr;
    try
    {
        pool = new threadpool<http_conn>(args);
    }
    catch(...)
    {
        log_err("fail make thread");
        return 1;
    }
    http_conn* users = new http_conn[MAX_FD];
    check_exit(users,"fail make users");
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    check_exit(listenfd>=0, "fail in socket");
    alarm(TIME_SLOT);
    // struct  linger tmp = {1,0};
    // setsockopt(listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(args.get_port());
    address.sin_addr.s_addr =  htonl(INADDR_ANY);
    ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address));
    check(ret>=0, "fail bind");
    assert(ret >= 0);
    ret = listen(listenfd, 5);
    check_exit(ret >= 0, "fail listen");
    epoll_event events[args.get_max_request()];
    int epollfd = epoll_create(5);
    check_exit(epollfd != -1, "fail epoll");
    addfd(epollfd, listenfd, false);
    addfd(epollfd, pipefd[0], false);
    setnonblock(pipefd[1]);
    http_conn::m_epollfd = epollfd;
    const int MAX_EVENTS_NUMBER  = args.get_max_request();
    while(true)
    {
        int number = epoll_wait(epollfd, events, MAX_EVENTS_NUMBER, -1);
        if((number < 0) && (errno != EINTR))
        {
            log_err("epoll fail");
            break;
        }
        for(int i=0;i<number;i++)
        {
            int sockfd = events[i].data.fd;
            if(sockfd == listenfd)
            {
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);
                int connfd = accept(listenfd, (sockaddr*)&client_address, &client_addrlength);
                log_info("accept connection %d", connfd);
                if(connfd < 0)
                {
                    log_err("accept error");
                    continue;
                }
                if(http_conn::m_user_count >= MAX_FD)
                {
                    log_info("close connection %d", connfd);
                    show_error(connfd, "internal server busy\n");
                    continue;
                }
                users[connfd].init(connfd, client_address);
            }
            else if(sockfd == pipefd[0])
            {
                char signal_r[1024];
                int rt;
                rt = recv(sockfd, signal_r, sizeof(signal_r), 0);
                if(rt <0)
                {
                    log_err("receive sockpair fail");
                    continue;
                }
                if(rt == 0)
                {
                    continue;
                }
                for(int i=0;i<rt;i++)
                {
                    switch(signal_r[i])
                    {
                        case SIGALRM:
                        {
                            timeout = true;
                            break;   
                        }
                    }
                }
            }
            else if(events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                log_info("close connection %d, client is closed", sockfd);
                users[sockfd].close_conn();
            }
            else if(events[i].events & EPOLLIN)
            {
                log_info("receive from fd %d", sockfd);
                if(users[sockfd].read())
                {
                    pool->append(users+sockfd);
                }
                else
                {
                    users[sockfd].close_conn();
                }
            }
            else if(events[i].events & EPOLLOUT)
            {
                if(!users[sockfd].write())
                {
                    users[sockfd].close_conn();
                }
            }
            
            else
            {

            }
        }
        if(timeout)
        {
            timeout = false;
            timer_handler();
        }
    }
close(epollfd);
close(listenfd);
close(pipefd[0]);
delete [] users;
delete pool;
return 0;
}