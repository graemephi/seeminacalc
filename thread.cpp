#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

#include "thread.h"

static std::vector<std::thread> threads;
static std::mutex m;
static std::condition_variable c;

int make_thread(int (fn)(void *), void *userdata)
{
    std::lock_guard<std::mutex> lock(m);
    threads.emplace_back(fn, userdata);
    threads.back().detach();
    return (int)threads.size() - 1;
}

int got_any_cores()
{
    return (int)std::thread::hardware_concurrency();
}

void wag_tail()
{
    std::atomic_thread_fence(std::memory_order_acq_rel);
}

void thread_wait()
{
    std::unique_lock<std::mutex> lock(m);
    c.wait(lock);
}

void thread_notify()
{
    std::lock_guard<std::mutex> lock(m);
    c.notify_all();
}

Lock *make_lock()
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
