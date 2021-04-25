# include "http_conn.h"
# include "timer.h"
using namespace std;
extern time_heap timer_http;
const char* ok_200_title = "OK";
const char* error_400_title = "Bad Request";
const char* error_400_form = "Your request has bad syntax or is inherently impossible to satifsfy.\n";
const char* error_403_title = "Forbidden";
const char* error_403_form = "You do not have permission to get file from this sever.\n";
const char* error_404_title = "Not Found";
const char* error_404_form = "The requested file was not found on this server.";
const char* error_500_title = "Internal Error";
const char* error_500_form = "There was an unusual problem serving the requested file.\n";
const char* doc_root = "../html";
const map<string,string> content_type_set{{".html", "text/html"},
    {".xml", "text/xml"},
    {".xhtml", "application/xhtml+xml"},
    {".txt", "text/plain"},
    {".rtf", "application/rtf"},
    {".pdf", "application/pdf"},
    {".word", "application/msword"},
    {".png", "image/png"},
    {".gif", "image/gif"},
    {".jpg", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".au", "audio/basic"},
    {".mpeg", "video/mpeg"},
    {".mpg", "video/mpeg"},
    {".avi", "video/x-msvideo"},
    {".gz", "application/x-gzip"},
    {".tar", "application/x-tar"},
    {".css", "text/css"},
    {"null" ,"text/plain"}};
int http_conn::m_user_count = 0;
int http_conn::m_epollfd = -1;
heap_timer::heap_timer(int delay, http_conn* http_m)
{
    http_instance = http_m;
    expire = time(NULL)+delay;
}
void http_conn::close_conn(bool close_conn_able)
{
    if(close_conn_able)
    {
        log_info("close fd %d",m_sockfd);
        if(m_sockfd != -1)
        {
            if(!timers_for_http.empty())
            {
                int size;
                size = timers_for_http.size();
                for(int i=0;i<size;i++)
                {
                    timer_http.del_timer(timers_for_http[i]);
                    delete timers_for_http[i];
                    
                }
                for(int i=0;i<size;i++)
                {
                    timers_for_http.pop_back();
                }
            }
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
    m_file_type = nullptr;
    m_linger = false;
    bzero(m_read_buf, READ_BUFFER_SIZE);
    bzero(m_write_buf, WRITE_BUFFER_SIZE);
    bzero(m_real_file, FILENAME_LEN);
    bzero(m_read_content, sizeof(m_read_content));
    
} 
void http_conn::init_header_map()
{
    m_header_map[0].header = "Host";
    m_header_map[0].fuc = bind(&http_conn::header_process_host, this, placeholders::_1);
    m_header_map[1].header = "Connection";
    m_header_map[1].fuc = bind(&http_conn::header_process_connection, this, placeholders::_1);
    m_header_map[2].header = "Content-Length";
    m_header_map[2].fuc = bind(&http_conn::header_process_content_length, this, placeholders::_1);
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
        debug("read content in %d is\n %s", m_sockfd, m_read_buf+m_read_idx);
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
http_conn::HTTP_CODE http_conn::parse_header(char* text)
{
    if(*text == '\0')
    {
        if(m_content_length != 0)
        {
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    int match = 0;
    for(int i=0;i<3;i++)
    {
        string header;
        header = m_header_map[i].header;
        if(strncasecmp(text, header.c_str(),header.size()) == 0)
        {
            match = 1;
            debug("header is %s", header.c_str());
            text = text+header.size();
            text++;
            while(*text == ' ')
            {
                text++;
            }
            m_header_map[i].fuc(text);
            break;
        }
    }
    if(match == 0)
    {
        debug("bad header:%s", text);
    }
    return NO_REQUEST;
}
void http_conn::header_process_host(char* text)
{
    m_host = text;
}
void http_conn::header_process_connection(char* text)
{
    if(strcasecmp(text,"keep-alive") == 0)
    {
        m_linger = true;
    }
}
void http_conn::header_process_content_length(char* text)
{
    m_content_length = atoi(text);
}
http_conn::HTTP_CODE http_conn::parse_content(char* text)
{
    if(m_read_idx > (m_content_length+m_checked_idx))
    {
        text[m_content_length] = '\0';
        return GET_REQUEST;
    }
    return NO_REQUEST;
}
http_conn::HTTP_CODE http_conn::process_read()
{
    HTTP_CODE ret = NO_REQUEST;
    char* text = 0;
    while((m_check_state == CHECK_STATE_CONTENT && parse_line() == LINE_OK) || (parse_line() == LINE_OK))
    {
        text =get_line();
        debug("get line is %s", text);
        m_start_line = m_checked_idx;
        switch(m_check_state)
        {
            case CHECK_STATE_REQUESTLINE:
            {
                ret = parse_request_line(text);
                if(ret == BAD_REQUEST)
                {
                    return BAD_REQUEST;
                }
                break;
            }
            case CHECK_STATE_HEADER:
            {
                ret = parse_header(text);
                if(ret == BAD_REQUEST)
                {
                    return BAD_REQUEST;
                }
                else if(ret == GET_REQUEST)
                {
                    return do_request();
                }
                break;
            }
            case CHECK_STATE_CONTENT:
            {
                ret = parse_content(text);
                if(ret == GET_REQUEST)
                {
                    return do_request();
                }
                break;
            }
            default:
            {
                return INTERNAL_ERROR;
            }
        }
    }
    return NO_REQUEST;
}
http_conn::HTTP_CODE http_conn::do_request()
{
    strcpy(m_real_file, doc_root);
    size_t len;
    len = strlen(doc_root);
    if(strcmp(m_url, "/") == 0)
    {
        strcpy(m_real_file+len, "/index.html");
    }
    else
    {
        strcpy(m_real_file+len, m_url);
    }
    if(stat(m_real_file, &m_file_stat) < 0) 
    {
        m_linger = false;
        return NO_RESOURCE;
    }
    if(! (m_file_stat.st_mode & S_IROTH))
    {
        m_linger = false;
        return FORBIDDEN_REQUEST;
    }
    if(S_ISDIR(m_file_stat.st_mode))
    {
        m_linger = false;
        return BAD_REQUEST;
    }
    int fd = open(m_real_file, O_RDONLY);
    m_file_address = (char*)mmap(NULL, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    m_file_type = strrchr(m_real_file, '.');
    log_info("file type is %s", m_file_type);
    m_file_time = m_file_stat.st_mtime;
    return FILE_REQUEST;
}
void http_conn::unmap()
{
    if(m_file_address)
    {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}
bool http_conn::write()
{
    int char_to_send;
    int char_have_send;
    char_to_send = m_write_idx;
    if(char_to_send == 0)
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        init();
        return true;
    }
    while(1)
    {
        int write_ret;
        int log_send_file;
        char* log_file;
        log_file = (char*) "./http_return";
        log_send_file = open(log_file, O_RDWR|O_CREAT|O_TRUNC, 0666);
        write_ret = writev(log_send_file, m_iv, m_iv_count);
        close(log_send_file);
        write_ret = 0;
        write_ret = writev(m_sockfd, m_iv, m_iv_count);
        if(write_ret <= -1)
        {
            if(errno == EAGAIN)
            {
                modfd(m_epollfd, m_sockfd, EPOLLOUT);
                return true;
            }
            else
            {
                unmap();
                return false;
            }
        }
        char_to_send = char_to_send-write_ret;
        char_have_send = char_have_send+write_ret;
        if(char_to_send <= char_have_send)
        {
            unmap();
            if(m_linger)
            {
                heap_timer* timer_new = new heap_timer(DEFAULT_TIME, this);
                timers_for_http.push_back(timer_new);
                timer_http.add_timer(timer_new);
                modfd(m_epollfd, m_sockfd, EPOLLIN);
                init();
                return true;
            }
            else
            {
                modfd(m_epollfd, m_sockfd, EPOLLIN);
                return false;
            }
        }
    }
}
bool http_conn::add_response(const char* format, ...)
{
    if(m_write_idx > WRITE_BUFFER_SIZE)
    {
        return false;
    }
    va_list argptr;
    va_start(argptr, format);
    int len;
    len = vsnprintf(m_write_buf+m_write_idx, WRITE_BUFFER_SIZE-m_write_idx-1,format, argptr);
    if(len >= READ_BUFFER_SIZE-m_write_idx-1)
    {
        return false;
    }
    m_write_idx = m_write_idx+len;
    va_end(argptr);
    return true;
}
bool http_conn::add_status_line(int status, const char* title)
{
    return add_response("%s %d %s \r\n","HTTP/1.1",status, title);
}
bool http_conn::add_content(const char* content)
{
    return add_response("%s",content);
}
bool http_conn::add_content_length(int content_len)
{
    return add_response("Content-Length: %d\r\n", content_len);
}
bool http_conn::add_headers(int content_length, int status)
{
    bool stat;
    stat = add_host_name();
    if(m_method != HEAD)
    {
        stat = stat & add_content_length(content_length);
    }
    stat = stat & add_linger();
    stat = stat & add_content_type();
    if(status == 200)
    {
        stat = stat & add_last_modified();
    }
    stat = stat & add_blank_line();
    return stat;
}
bool http_conn::add_host_name()
{
    return add_response("sever: %s\r\n", "http_sever 1.0");
}
bool http_conn::add_linger()
{
    char* str;
    if(m_linger)
    {
        str = (char*) "keep-alive";
        add_response("keep-alive: %d\r\n", DEFAULT_TIME);
    }
    else
    {
        str = (char*)"close";
    }
    return add_response("Connection: %s\r\n", str);
}
bool http_conn::add_content_type()
{
    if(m_file_type == nullptr)
    {
        return add_response("Content-type: %s\r\n","text/plain");
    }
    auto ptr = content_type_set.find(m_file_type);
    if(ptr != content_type_set.end())
    {
        return add_response("Content-type: %s\r\n", (ptr->second).c_str());
    }
    else
    {
        return add_response("Content-type:%s\r\n", "text/plain");
    }
    return false;
}
string http_conn::uniform_content(const char* content)
{
    string str;
    str = str+"<html><title>Server Error</title><body bgcolor=ffffff>\n";
    str = str+content;
    str = str+"</p><hr><em>Http web server</em>\n";
    str = str+"</body></html>\n";
    return str;
}
bool http_conn::add_last_modified()
{
    tm tm_s;
    char mod_t[512];
    localtime_r(&m_file_time, &tm_s);
    strftime(mod_t, 512,"%a, %d %b %Y %H:%M:%S GMT", &tm_s );
    return add_response("Last-Modified: %s\r\n", mod_t);
}
bool http_conn::add_blank_line()
{
    return add_response("%s", "\r\n");
}
bool http_conn::process_write(HTTP_CODE ret)
{
    switch(ret)
    {
        case INTERNAL_ERROR:
        {
            string str;
            add_status_line(500, error_500_title);
            str = uniform_content(error_500_form);
            add_headers(str.size(), 500);
            if(! add_content(str.c_str()))
            {
                return false;
            }
            break;
        }
        case BAD_REQUEST:
        {
            string str;
            add_status_line(400, error_400_title);
            str = uniform_content(error_400_form);
            add_headers(str.size(),400);
            if(m_method != HEAD)
            {
                if(!add_content(str.c_str()))
                {
                    return false;
                }
            }
            break;
        }
        case NO_RESOURCE:
        {
            string str;
            add_status_line(404, error_404_title);
            str = uniform_content(error_404_form);
            add_headers(str.size(),404);
            if(m_method != HEAD)
            {
                if(! add_content(str.c_str()))
                {
                    return false;
                }
            }
            break;
        }
        case FORBIDDEN_REQUEST:
        {
            string str;
            add_status_line(403, error_403_title);
            str = uniform_content(error_403_form);
            add_headers(str.size(), 403);
            if(m_method != HEAD)
            {
                if(! add_content(str.c_str()))
                {
                    return false;
                }
            }
            break;
        }
        case FILE_REQUEST:
        {
            add_status_line(200, ok_200_title);
            if(m_file_stat.st_size != 0)
            {
                add_headers(m_file_stat.st_size, 200);
                m_iv[0].iov_base = m_write_buf;
                m_iv[0].iov_len = strlen(m_write_buf);
                if(m_method == HEAD)
                {
                    m_iv_count = 1;
                }
                else
                {
                    m_iv[1].iov_base = m_file_address;
                    m_iv[1].iov_len = m_file_stat.st_size;
                    m_iv_count = 2;
                }
                return true;
            }
            else
            {
                char* str = (char*) "<html><body></body></html>";
                add_headers(strlen(str),200);
                if(! add_content(str))
                {
                    return false;
                }
                break;
            }
        }
        default:
        {
            return false;
        }
    }
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = strlen(m_write_buf);
    m_iv_count = 1;
    return true;
}
void http_conn::set_timer_handler(function<void()> fuc)
{
    timer_handler = fuc;
}
http_conn::~http_conn()
{
    if(! timers_for_http.empty())
    {
        int size;
        size = timers_for_http.size();
        for(int i=0;i<size;i++)
        {
            delete timers_for_http[i];
        }
    }
}
void http_conn::process()
{
    HTTP_CODE ret;
    ret = process_read();
    if(ret == NO_REQUEST)
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        return;
    }
    bool sta;
    sta = process_write(ret);
    if(sta == false)
    {
        debug("close num %d", m_sockfd);
        close_conn();
        return;
    }
    modfd(m_epollfd, m_sockfd, EPOLLOUT);

}





