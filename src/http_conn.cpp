# include "http_conn.h"

using namespace std;
const char* ok_200_title = "OK";
const char* error_400_title = "Bad Request";
const char* error_400_form = "Your request has bad syntax or is inherently impossible to satifsfy.\n";
const char* error_403_title = "Forbidden";
const char* error_403_form = "You do not have permission to get file from this sever.\n";
const char* error_404_title = "Not Found";
const char* error_404_form = "The requested file was not found on this server.";
const char* error_500_title = "Internal Error";
const char* error_500_form = "There was an unusual problem serving the requested file.\n";
const char* doc_root = "/home/billy/code/net_check/http_server/html";
int http_conn::m_user_count = 0;
int http_conn::m_epollfd = -1;
void http_conn::close_conn(bool close_conn_able)
{
    if(close_conn_able)
    {
        if(m_sockfd != -1)
        {
            deletefd(m_epollfd, m_sockfd);
            m_sockfd = -1;
            m_user_count--;
        }
    }
}
void http_conn::init(int sockfd, const sockaddr_in &addr)
{
    m_sockfd = sockfd;
    m_address = addr;
    addfd(m_epollfd, m_sockfd, true);
    m_user_count++;
    init();
    init_header_map();
}
void http_conn::init()
{
    timer_handler =  bind(&http_conn::close_conn,this,true);
    m_read_idx = 0;
    m_checked_idx = 0;
    m_start_line = 0;
    m_write_idx = 0;
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_host = 0;
    m_content_length = 0;
    m_linger = false;
    bzero(m_read_buf, READ_BUFFER_SIZE);
    bzero(m_write_buf, WRITE_BUFFER_SIZE);
    bzero(m_real_file, FILENAME_LEN);
    bzero(m_read_content, sizeof(m_read_content));
    
} 
void http_conn::init_header_map()
{
    m_header_map[0].header = "Host";
    m_header_map[0].fuc = bind(&http_conn::header_ignore, this);
    m_header_map[1].header = "Connection";
    m_header_map[1].fuc = bind(&http_conn::header_process_connection, this);
    m_header_map[2].header = "";
    m_header_map[2].fuc = bind(&http_conn::header_ignore, this);
}
http_conn::LINE_STATUS http_conn::parse_line()
{
    for(;m_checked_idx < m_read_idx;m_checked_idx++)
    {
        if(m_read_buf[m_checked_idx] == CR)
        {
            if(m_read_buf[m_checked_idx+1] != m_read_idx)
            {
                if(m_read_buf[m_checked_idx+1] == LF)
                {
                    m_read_buf[m_checked_idx++] = '\0';
                    m_read_buf[m_checked_idx++] = '\0';
                    return LINE_OK;
                }
                else
                {
                    return LINE_BAD;
                }
            }
            else
            {
                return LINE_OPEN;
            }
        }
        else if(m_read_buf[m_checked_idx] == LF)
        {
            if(m_checked_idx>1 && m_read_buf[m_checked_idx-1] == CR)
            {
                m_read_buf[m_checked_idx-1] = '\0';
                m_read_buf[m_checked_idx--] = '\0';
                return LINE_OK;
            }
            else
            {
                return LINE_BAD;
            }
        }
    }
    return LINE_OPEN;
}
bool http_conn::read()
{
    int read_len;
    while(1)
    {
        read_len = recv(m_sockfd, m_read_buf+m_read_idx,READ_BUFFER_SIZE-m_read_idx,0);
        debug("read content in %d is %s", m_sockfd, m_read_buf+m_read_idx);
        if(read_len < 0)
        {
            if(errno == EAGAIN || errno == EWOULDBLOCK)
            {
                break;
            }
            return false;
        }
        else if(read_len == 0)
        {
            return false;
        }
        strcpy(m_read_content+m_read_idx, m_read_buf+m_read_idx);
        m_read_idx += read_len;
    }
    return true;
}
http_conn::HTTP_CODE http_conn::parse_request_line(char* text)
{
    char* temp;
    temp = text;
    char* url;
    url =strpbrk(temp, " ");
    if(url == NULL)
    {
        return BAD_REQUEST;
    }
    *url++ = '\0';
    if(strcasecmp(text, "GET") == 0)
    {
        m_method = GET;
    }
    else if(strcasecmp(text, "HEAD") == 0)
    {
        m_method = HEAD;
    }
    else if(strcasecmp(text, "TRACE") == 0)
    {
        m_method = TRACE;
    }
    else
    {
        return BAD_REQUEST;
    }
    char* version;
    version = strpbrk(url ," ");
    if(version != NULL)
    {
        *version++ = '\0';
    }
    else
    {
        return BAD_REQUEST;
    }
    if(strcasecmp(version, "HTTP/1.1") != 0)
    {
        return BAD_REQUEST;
    }
    m_version = version;
    m_url = url;
    if(strncasecmp(m_url, "http://",7) == 0)
    {
        m_url += 7;
        if(strcmp(m_url, "") == 0)
        {
            *m_url = '/';
        }
        else
        {
            m_url = strchr(m_url,'/');
            if(m_url == NULL ||m_url[0] != '/')
            {
                return BAD_REQUEST;
            }
        }
    }
    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}