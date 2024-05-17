/**
 * File: thread-pool.cc
 * --------------------
 * Presents the implementation of the ThreadPool class.
 */

#include "thread-pool.h"
using namespace std;

ThreadPool::ThreadPool(size_t numThreads) : worker_sem(0), dispatcher_sem(0) {
    dt = thread([this]() { dispatcher(); });
    wts.resize(numThreads);
    for (size_t i = 0; i < numThreads; ++i) {
        wts[i] = thread([this]() { worker(); });
    }
}

void ThreadPool::schedule(const function<void(void)>& thunk) {
    {
        lock_guard<mutex> lock(mtx);
        thunks.push_back(thunk);
    }
    cv.notify_one();
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
    cv.notify_all();

    dt.join();
    for (size_t i = 0; i < wts.size(); ++i) {
        worker_sem.signal();
        wts[i].join();
    }
}

void ThreadPool::dispatcher() {
    while (true){
        unique_lock<mutex> lock(mtx);
        cv.wait(lock, [this]() { return done || !thunks.empty(); });
        if (done) break;

        if (!thunks.empty()) {
            worker_sem.signal();
        }
    }

}

void ThreadPool::worker() {
    while (true){
        worker_sem.wait();
        if (done) break;

        function <void(void)> thunk;
        {
            lock_guard<mutex> lock(mtx);
            if (!thunks.empty()) {
                thunk = thunks.front();
                thunks.pop_back();
                ++active_workers;
            } else{
                continue;
            }
        }

        thunk();
        {
            lock_guard<mutex> lock(mtx);
            --active_workers;
            if (thunks.empty() && active_workers == 0) {
                cv.notify_all();
            }
        }
    }
}