/**
 * File: thread-pool.cc
 * --------------------
 * Presents the implementation of the ThreadPool class.
 */

#include "thread-pool.h"
using namespace std;

ThreadPool::ThreadPool(size_t numThreads) 
    : worker_sem(0), dispatcher_sem(numThreads), done(false), workers(numThreads) {
    for (size_t i = 0; i < numThreads; i++) {
        wts.push_back(thread([this, i] { worker(i); }));
    }
    dt = thread([this] { dispatcher(); });
}

void ThreadPool::schedule(const function<void(void)>& thunk) {
    {
        unique_lock<mutex> lock(mtx);
        thunks.push(thunk);
    }
    cv.notify_one();
}

void ThreadPool::wait() {
    unique_lock<mutex> lock(mtx);
    cv.wait(lock, [this]() {
        return thunks.empty() && all_of(workers.begin(), workers.end(), [](const Worker& w) { return !w.occupied; });
    });

    for (auto& wt : wts) {
        worker_sem.signal();
        wt.join();
    }

    for (auto& worker : workers) {
        worker.occupied = false;
    }
}

ThreadPool::~ThreadPool() {
    {
        unique_lock<mutex> lock(mtx);
        done = true;
    }
    cv.notify_all();
    dt.join();

    for (auto& wt : wts) {
        worker_sem.signal();
        wt.join();
    }
}

void ThreadPool::worker(size_t workerid) {
    while (true) {
        function<void()> task;
        {
            unique_lock<mutex> lock(mtx);
            workers[workerid].cv_worker.wait(lock, [this, workerid] {
                return workers[workerid].occupied || done;
            });

            if (done && !workers[workerid].occupied) {
                return;
            }

            task = workers[workerid].thunk;
        }

        task();

        {
            lock_guard<mutex> lock(mtx);
            workers[workerid].occupied = false;
            dispatcher_sem.signal();
        }
    }
}

void ThreadPool::dispatcher() {
    while (true) {
        unique_lock<mutex> lock(mtx);
        cv.wait(lock, [this] {
            return !thunks.empty() || done;
        });

        if (done && thunks.empty()) {
            return;
        }

        if (!thunks.empty()) {
            dispatcher_sem.wait();

            for (size_t i = 0; i < workers.size(); ++i) {
                if (!workers[i].occupied) {
                    workers[i].thunk = thunks.front();
                    thunks.pop();
                    workers[i].occupied = true;
                    workers[i].cv_worker.notify_one();
                    break;
                }
            }
        }
    }
}