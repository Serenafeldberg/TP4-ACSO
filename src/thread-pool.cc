/**
 * File: thread-pool.cc
 * --------------------
 * Presents the implementation of the ThreadPool class.
 */

#include "thread-pool.h"
using namespace std;

ThreadPool::ThreadPool(size_t numThreads) : sem(0), active_workers(0), done(false) {
    for (size_t i = 0; i < numThreads; ++i) {
        wts.emplace_back(&ThreadPool::worker, this);
    }
}

void ThreadPool::schedule(const function<void(void)>& thunk) {
    {
        lock_guard<mutex> lock(mtx);
        thunks.push_back(thunk);
    }
    sem.signal(); 
}

void ThreadPool::wait() {
    unique_lock<mutex> lock(mtx);
    cv.wait(lock, [this]() { return thunks.empty() && active_workers == 0; });

}

ThreadPool::~ThreadPool() {
    {
        lock_guard<mutex> lock(mtx);
        done = true;
    }
    for (size_t i = 0; i < wts.size(); ++i) {
        sem.signal();
    }

    for (auto& wt : wts) {
        if (wt.joinable()) {
            wt.join();
        }
    }
}

void ThreadPool::worker() {
    while (true) {
        sem.wait();
        function<void(void)> thunk;
        {
            lock_guard<mutex> lock(mtx);
            if (done) return;
            if (thunks.empty()) {
                sem.signal();
                continue;
            }
            thunk = thunks.back();
            thunks.pop_back();
            ++active_workers;
        }
        thunk();
        {
            lock_guard<mutex> lock(mtx);
            --active_workers;
            if (thunks.empty() && active_workers == 0) {
                cv.notify_one();
            }
        }
    }
}