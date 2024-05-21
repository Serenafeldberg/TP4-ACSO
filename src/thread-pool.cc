/**
 * File: thread-pool.cc
 * --------------------
 * Presents the implementation of the ThreadPool class.
 */

#include "thread-pool.h"
using namespace std;

ThreadPool::ThreadPool(size_t numThreads) 
    : wts(numThreads),  worker_sem(0), dispatcher_sem(numThreads), done(false), workers(numThreads) {
    for (size_t i = 0; i < numThreads; i++) {
        wts[i] = thread([this, i] { worker(i); });
        worker_sem.signal();
    }
    dt = thread([this] { dispatcher(); });
}

void ThreadPool::schedule(const function<void(void)>& thunk) {
    {
        unique_lock<mutex> lock(mtx);
        thunks.push(thunk);
    }
    dispatcher_sem.signal();
}

void ThreadPool::wait() {
    unique_lock<mutex> lock(mtx);
    cv.wait(lock, [this]() {
        return thunks.empty() && active_workers == 0;
    });
}

ThreadPool::~ThreadPool() {
    {
        unique_lock<mutex> lock(mtx);
        done = true;
    }
    dispatcher_sem.signal();

    for (auto& w: workers) {
        w.cv_worker.notify_one();
    }

    for (auto& wt : wts) {
        if (wt.joinable()) {
            wt.join();
        }
    }

    if (dt.joinable()) {
        dt.join();
    }
}

void ThreadPool::worker(size_t workerid) {
    while (! (done && thunks.empty())) {
        function<void()> task;
        {
            unique_lock<mutex> lock(mtx);
            workers[workerid].cv_worker.wait(lock, [this, workerid] {
                return workers[workerid].occupied || done;
            });
            if (done && thunks.empty()) {
                break;
            }
            task = move(workers[workerid].thunk);

        }
        if (task) {
            workers[workerid].occupied = true;
            task();
            {
                unique_lock<mutex> lock(mtx);
                workers[workerid].occupied = false;
                worker_sem.signal();
                active_workers--;
                if (thunks.empty() && active_workers == 0) {
                    cv.notify_all();
                }
            }
        }
    }
}

void ThreadPool::dispatcher() {
    while (!done) {
        dispatcher_sem.wait();
        function<void()> task;
        {
            unique_lock<mutex> lock(mtx);
            if (done && thunks.empty()) {
                break;
            }
            if (!thunks.empty()) {
                task = move(thunks.front());
                thunks.pop();
            }
        }
        if (task) {
            worker_sem.wait();
            for (size_t i = 0; i < wts.size(); ++i) {
                unique_lock<mutex> lock(mtx);
                if (!workers[i].occupied) {
                    workers[i].thunk = task;
                    workers[i].occupied = true;
                    workers[i].cv_worker.notify_one();
                    active_workers++;
                    break;
                }
            }
        }
    }


    // while (true) {
    //     {
    //         unique_lock<mutex> lock(mtx);
    //         cv.wait(lock, [this] {
    //             return !thunks.empty() || done;
    //         });

    //         if (done && thunks.empty()) {
    //             return;
    //         }

    //         if (!thunks.empty()) {
    //             dispatcher_sem.wait();

    //             for (size_t i = 0; i < workers.size(); ++i) {
    //                 if (!workers[i].occupied) {
    //                     workers[i].thunk = thunks.front();
    //                     thunks.pop();
    //                     workers[i].occupied = true;
    //                     workers[i].cv_worker.notify_one();
    //                     break;
    //                 }
    //             }
    //         }
    //     }
    // }
}
