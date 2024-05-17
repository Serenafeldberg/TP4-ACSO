/**
 * File: thread-pool.cc
 * --------------------
 * Presents the implementation of the ThreadPool class.
 */

#include "thread-pool.h"
using namespace std;

ThreadPool::ThreadPool(size_t numThreads) : sem(0), done(false), active_workers(0) {
    dt = thread ([this] {dispatcher();});
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
    sem.signal();
    dt.join();
    for (size_t i = 0; i < wts.size(); ++i) {
        sem.signal();
    }

    for (auto& wt : wts) {
        if (wt.joinable()) {
            wt.join();
        }
    }
}

void ThreadPool::dispatcher() {
    while (true) {
        sem.wait();
        function<void(void)> thunk;
        {
            unique_lock<mutex> lock(mtx);
            if (done && thunks.empty()) return;
            if (!thunks.empty()) {
                thunk = thunks.back();
                thunks.pop_back();
            }
        }
        if (thunk){
            {
                lock_guard<mutex> lock(mtx);
                ++active_workers;
            }
            thread(&ThreadPool::worker, this, thunk).detach();
        }
    }

}

void ThreadPool::worker(function<void(void)> thunk) {
    if (thunk){
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