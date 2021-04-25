# include "timer.h"
using namespace std;
time_heap::time_heap(int cap): capacity(cap), cur_size(0)
{
    array = new heap_timer*[cap];
    if(! array)
    {
        log_err("timer instance failure.");
        exit(1);
    }
    for(int i=0;i<capacity;i++)
    {
        array[i] = nullptr;
    }
}
time_heap::time_heap(heap_timer** init_array, int size, int cap):
cur_size(size), capacity(cap)
{
    if(capacity < size)
    {
        log_err("capacity %d less than size %d", capacity, size);
        exit(1);
    }
    array = new heap_timer*[cap];
    for(int i=0;i<capacity;i++)
    {
        array[i] = nullptr;
    }
    for(int i=0;i<cur_size;i++)
    {
        array[i] = init_array[i];
    }
    for(int i=(cur_size-1)/2;i>=0;i--)
    {
        percolate_down(i);
    }
}
time_heap::~time_heap()
{
    for(int i=0;i<cur_size;i++)
    {
        delete array[i];
    }
    delete [] array;
}
void time_heap::add_timer(heap_timer* timer)
{
    if(! timer)
    {
        log_err("timer is null");
        exit(1);
    }
    if(cur_size >= capacity)
    {
        resize();
    }
    int hole = cur_size++;
    int parent = 0;
    for(;hole>0;hole = parent)
    {
        parent = (hole-1)/2;
        if(array[parent]->expire <= timer->expire)
        {
            break;
        }
        array[hole] = array[parent];
    }
    array[hole] = timer;
}
void time_heap::del_timer(heap_timer* timer)
{
    if(!timer)
    {
        return;
    }
    timer->stop_timer = true;
}
heap_timer* time_heap::top() const
{
    if(empty())
    {
        return nullptr;
    }
    return array[0];
}
void time_heap::pop_timer()
{
    if(empty())
    {
        return;
    }
    if(array[0])
    {
        //delete array[0];
        array[0] = array[--cur_size];
        percolate_down(0);
    }
}
void time_heap::tick()
{
    heap_timer* temp;
    temp = array[0];
    while(! empty())
    {
        time_t cur;
        cur = time(NULL);
        if(temp == nullptr)
        {
            break;
        }
        if(cur < temp->expire)
        {
            break;
        }
        if(array[0]->http_instance && array[0]->stop_timer == false)
        {
            array[0]->http_instance->timer_handler();
            // timer_handler is a memeber of class http_conn type std::function, and use member function  set_timer_handler in class http_conn to set the handler.
        }
        pop_timer();
        temp = array[0];
    }
}
void time_heap::percolate_down(int hole)
{
    int child = 0;
    heap_timer* temp;
    temp = array[hole];
    for(;2*hole+1<cur_size;hole = child)
    {
        child = 2*hole+1;
        if(2*hole+2 < cur_size && array[child+1]->expire < array[child]->expire)
        {
            child = child+1;
        }
        if(array[child]->expire < temp->expire)
        {
                array[hole] = array[child];
        }
        else
        {
            break;
        }
    }
        array[hole] = temp;
}
void time_heap::resize()
{
    heap_timer** temp = new heap_timer* [2*capacity];
    for(int i=0;i<capacity;i++)
    {
        temp[i] = nullptr;
    }
    for(int i=0;i<cur_size;i++) 
    {
        temp[i] = array[i];
    }
    for(int i=0;i<cur_size;i++)
    {
        delete array[i];
    }
    delete[] array;
    array = temp;
    capacity = 2*capacity;
}
time_heap timer_http(10);



