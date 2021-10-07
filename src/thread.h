#ifdef __cplusplus
extern "C"
{
typedef std::mutex Lock;
#else
typedef struct Lock Lock;
#endif

int make_thread(int (fn)(void *), void *userdata);
int got_any_cores(void);
void wag_tail(void);
void thread_wait(void);
void thread_notify(void);
Lock *make_lock(void);
void lock(Lock *lock);
void unlock(Lock *lock);

#ifdef __cplusplus
}
#endif
