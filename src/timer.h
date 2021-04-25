#  ifndef TIMER_H
# define TIMER_H
# include <iostream>
# include <netinet/in.h>
# include <time.h>
# include "http_conn.h"
# include "logging.h"
using namespace std;

class heap_timer;
class time_heap;
class time_heap
{
    public:
        time_heap(int);
        time_heap(heap_timer**, int, int);
        ~time_heap();
        void add_timer(heap_timer*);
        void del_timer(heap_timer*);
        heap_timer* top() const;
        void pop_timer();
        void tick();
        bool empty() const {return cur_size == 0;}
    private:
        void percolate_down(int);
        void resize();
        heap_timer** array;
        int capacity;
        int cur_size;
};
# endif