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
    done = true;
    cv.notify_all();

    dt.join();
    for (size_t i = 0; i < wts.size(); ++i) {
        wts[i].join();
    }
}

void ThreadPool::dispatcher() {
    while (!done){
        function<void(void)> thunk;
        {
            unique_lock<mutex> lock(mtx);
            cv.wait(lock, [this]() { return done || !thunks.empty(); });
            if (done && thunks.empty()) return;
            if (!thunks.empty()) {
                thunk = thunks.front();
                thunks.pop_back();
            }
        }
        if (thunk) {
            worker_sem.wait();
            thread([this, thunk]() {
                thunk();
                dispatcher_sem.signal();
            }).detach();

        }
    }

}

void ThreadPool::worker() {
    while (!done) {
        dispatcher_sem.signal();
        worker_sem.wait();
        {
            lock_guard<mutex> lock(mtx);
            active_workers++;
        }
        dispatcher_sem.wait();
        if (!done) {
            function<void(void)> thunk;
            {
                lock_guard<mutex> lock(mtx);
                if (!thunks.empty()) {
                    thunk = thunks.front();
                    thunks.pop_back();
                }
            }
            if (thunk) {
                thunk();
            }
            {
                lock_guard<mutex> lock(mtx);
                active_workers--;
                cv.notify_one();
            }
        }
    }
}