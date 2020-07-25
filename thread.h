#ifdef __cplusplus
extern "C"
{
#endif

int make_thread(int (fn)(void *), void *userdata);
int got_any_cores(void);
void wag_tail(void);
void thread_wait(void);
void thread_notify(void);
int make_lock(void);
void lock(int lock);
void unlock(int lock);

#ifdef __cplusplus
}
#endif
