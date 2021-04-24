# ifndef LOCKER_H
# define LOCKER_H
# include <exception>
# include <semaphore.h>
class sem;
class sem
{
    public:
        sem();
        ~sem();
        bool wait();
        bool post();
    private:
        sem_t m_sem;
};
# endif