#ifdef __cplusplus
extern "C"
{
typedef std::mutex Lock;
#else
typedef struct Lock Lock;
#endif


int make_thread(int (fn)(void *), void *userdata);
int got_any_cores(void);
void memory_barrier(void);

Lock *lock_create(void);
void lock(Lock *lock);
void unlock(Lock *lock);

typedef int BadSem;
BadSem sem_create(void);
void sem_wait(BadSem sem);
void sem_notify(BadSem sem);

#ifdef __cplusplus
}
#endif
