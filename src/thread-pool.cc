/**
 * File: thread-pool.cc
 * --------------------
 * Presents the implementation of the ThreadPool class.
 */

#include "thread-pool.h"
using namespace std;

ThreadPool::ThreadPool(size_t numThreads) : sem(0) {
  dt = std::thread([this]() { dispatcher(); });        // create dispatcher thread
    wts.resize(numThreads);
    for (size_t i = 0; i < numThreads; ++i) {
        wts[i] = std::thread([this]() { worker(); });  // create worker threads
    }
}

void ThreadPool::schedule(const function<void(void)>& thunk) {
    thunks.push_back(thunk);
    sem.signal(); 
}

void ThreadPool::wait() {
    for (size_t i = 0; i < wts.size(); ++i) {
        sem.wait();
    }
}

ThreadPool::~ThreadPool() {
    done = true;
    sem.signal();

    dt.join();
    for (size_t i = 0; i < wts.size(); ++i) {
        wts[i].join();
    }
}

void ThreadPool::dispatcher() {
    while (!done) {
        sem.wait();  // wait

        worker();   // available worker
    }
}

void ThreadPool::worker() {
    while (!done) {
        sem.wait();        // wait

        thunks.back()();   // execute thunk
        thunks.pop_back();

        sem.signal();      // notify dispatcher
    }
}