#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

#include "thread.h"

static std::vector<std::thread> threads;
static std::vector<std::mutex> muts(2);
static int mut_index = 0;
static std::mutex m;
static std::condition_variable c;

int make_thread(int (fn)(void *), void *userdata)
{
    std::lock_guard lock(m);
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
    std::unique_lock lock(m);
    c.wait(lock);
}

void thread_notify()
{
    std::lock_guard lock(m);
    c.notify_all();
}

int make_lock()
{
    if (mut_index < muts.size()) {
        return ++mut_index;
    }

    assert_unreachable();
    return 0;
}

void lock(int lock_id)
{
    muts[lock_id - 1].lock();
}

void unlock(int lock_id)
{
    muts[lock_id - 1].unlock();
}
