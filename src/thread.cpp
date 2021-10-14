#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "thread.h"

static std::vector<std::thread> threads;
static std::vector<std::unique_ptr<std::mutex>> sem_mut;
static std::vector<std::unique_ptr<std::condition_variable>> sem_cond;

int make_thread(int (fn)(void *), void *userdata)
{
    threads.emplace_back(fn, userdata);
    threads.back().detach();
    return (int)threads.size() - 1;
}

int got_any_cores()
{
    return (int)std::thread::hardware_concurrency();
}

void memory_barrier()
{
    std::atomic_thread_fence(std::memory_order_acq_rel);
}

Lock *lock_create()
{
    return new std::mutex;
}

void lock(Lock *lock)
{
    lock->lock();
}

void unlock(Lock *lock)
{
    lock->unlock();
}

BadSem sem_create(void)
{
    int n = (int)sem_mut.size();
    sem_mut.push_back(std::make_unique<std::mutex>());
    sem_cond.push_back(std::make_unique<std::condition_variable>());
    return n;
}

void thread_wait(BadSem sem)
{
    std::unique_lock<std::mutex> lock(*sem_mut[sem]);
    sem_cond[sem]->wait(lock);
}

void thread_notify(BadSem sem)
{
    std::lock_guard<std::mutex> lock(*sem_mut[sem]);
    sem_cond[sem]->notify_all();
}
