/**
 * File: thread-pool.cc
 * --------------------
 * Presents the implementation of the ThreadPool class.
 */

#include "thread-pool.h"
using namespace std;

ThreadPool::ThreadPool(size_t numThreads) : worker_sem(0), dispatcher_sem(0) {
    workers.resize(numThreads);
    dt = thread([this]() { dispatcher(); });          // dispatcher thread
    wts.resize(numThreads);
    for (size_t i = 0; i < numThreads; ++i) {
        wts[i] = thread([this, i]() { worker(i); });  // worker thread
    }
}

void ThreadPool::schedule(const function<void(void)>& thunk) {
    {
        lock_guard<mutex> lock(mtx);
        thunks.push(thunk); 
    }
    cv.notify_one(); 
}

void ThreadPool::wait() {
    {
        unique_lock<mutex> lock(mtx);
        cv.wait(lock, [this]() { return thunks.empty() && all_of(workers.begin(), workers.end(), [](const Worker& w) { return !w.occupied; }); });
    
    }

}

ThreadPool::~ThreadPool() {
    {
        lock_guard<mutex> lock(mtx);
        done = true;
    }
    cv.notify_all(); 

    dt.join();
    for (auto& wt : wts) {
        worker_sem.signal();
        wt.join();
    }
}

void ThreadPool::dispatcher() {
    while (true) {
        unique_lock<mutex> lock(mtx);
        cv.wait(lock, [this]() { return !thunks.empty() || done; });

        if (done && thunks.empty()) break;

        for (size_t i = 0; i < workers.size(); ++i) {
            if (!workers[i].occupied && !thunks.empty()) {
                workers[i].thunk = thunks.front();
                thunks.pop();
                workers[i].occupied = true;
                workers[i].cv_worker.notify_one(); 
                break;
            }
        }
    }
}

void ThreadPool::worker(size_t workerid) {
    while (true) {
        unique_lock<mutex> lock(mtx);
        workers[workerid].cv_worker.wait(lock, [this, workerid]() { return workers[workerid].occupied || done; });

        if (done) break;

        if (workers[workerid].occupied) {
            workers[workerid].thunk();  
            workers[workerid].occupied = false;

            {
                std::lock_guard<std::mutex> lock(mtx);
                if (thunks.empty() && all_of(workers.begin(), workers.end(), [](const Worker& w) { return !w.occupied; })) {
                    cv.notify_all(); 
                }
            }
        }
    }
}