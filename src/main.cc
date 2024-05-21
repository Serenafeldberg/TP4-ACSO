#include "thread-pool.h"
#include <iostream>
#include <vector>
#include <numeric>  // For std::accumulate
#include <functional>

using namespace std;

// TP custom test

#include <sstream>
#include <map>
#include <string>
#include <functional>
#include <cstring>
#include <mutex>
#include <sys/types.h> // used to count the number of threads
#include <unistd.h>    // used to count the number of threads
#include <dirent.h>    // for opendir, readdir, closedir

void sleep_for(int slp){
    this_thread::sleep_for(chrono::milliseconds(slp));
}

static mutex oslock;

static const size_t kNumThreads = 4;
static const size_t kNumFunctions = 10;
static void simpleTest() {
  ThreadPool pool(kNumThreads);
  for (size_t id = 0; id < kNumFunctions; id++) {
    pool.schedule([id] {
      oslock.lock();
      cout << "Thread (ID: " << id << ") has started." << endl;
      oslock.unlock();
      size_t sleepTime = (id % 3) * 10;
      sleep_for(sleepTime);
      oslock.lock();
      cout <<  "Thread (ID: " << id << ") has finished." << endl ;
      oslock.unlock();
    });
  }

  pool.wait();
}


static void singleThreadNoWaitTest() {
    ThreadPool pool(4);

    pool.schedule([&] {
        oslock.lock();
        cout << "This is a test." << endl;
        oslock.unlock();
    });
    sleep_for(1000); // emulate wait without actually calling wait (that's a different test)
}

static void singleThreadSingleWaitTest() {
    ThreadPool pool(4);
    pool.schedule([] {
        oslock.lock();
        cout << "This is a test." << endl;
        oslock.unlock();
        sleep_for(1000);
    });
}

static void noThreadsDoubleWaitTest() {
    ThreadPool pool(4);
    pool.wait();
    pool.wait();
}

static void reuseThreadPoolTest() {
    ThreadPool pool(4);
    for (size_t i = 0; i < 16; i++) {
        pool.schedule([] {
            oslock.lock();
            cout << "This is a test." << endl;
            oslock.unlock();
            sleep_for(50);
        });
    }
    pool.wait();
    pool.schedule([] {
        oslock.lock();
        cout << "This is a code." << endl;
        oslock.unlock();
        sleep_for(1000);
    }); 
    pool.wait();
}

struct testEntry {
    string flag;
    function<void(void)> testfn;
};

static void buildMap(map<string, function<void(void)>>& testFunctionMap) {
    testEntry entries[] = {
        {"--single-thread-no-wait", singleThreadNoWaitTest},
        {"--single-thread-single-wait", singleThreadSingleWaitTest},
        {"--no-threads-double-wait", noThreadsDoubleWaitTest},
        {"--reuse-thread-pool", reuseThreadPoolTest},
        {"--s", simpleTest},
    };

    for (const testEntry& entry: entries) {
        testFunctionMap[entry.flag] = entry.testfn;
    }
}

static void executeAll(const map<string, function<void(void)>>& testFunctionMap) {
    for (const auto& entry: testFunctionMap) {
        cout << entry.first << ":" << endl;
        entry.second();
    }
}

int tpcustomtest (int argc, char *argv[]) {
    if (argc != 2) {
        cout << "Ouch! I need exactly two arguments." << endl;
        return 0;
    }

    map<string, function<void(void)>> testFunctionMap;
    buildMap(testFunctionMap);
    string flag = argv[1];
    if (flag == "--all") {
        executeAll(testFunctionMap);
        return 0;
    }
    auto found = testFunctionMap.find(argv[1]);
    if (found == testFunctionMap.end()) {
        cout << "Oops... we don't recognize the flag \"" << argv[1] << "\"." << endl;
        return 0;
    }

    found->second();
    return 0;
}

// End of TP custom test

// Function to compute the sum of a subvector
void computeSum(const vector<int>& data, int start, int end, int* result) {

    *result = accumulate(data.begin() + start, data.begin() + end, 0);
}

int main() {
    // Sample data
    vector<int> data = {100, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    int numThreads = 3;
    ThreadPool pool(numThreads);

    // Results vector to hold the sums computed by each thread
    vector<int> results(numThreads, 0);

    // Determine the size of each chunk of data to process
    int n = data.size();
    int chunkSize = (n + numThreads - 1) / numThreads;  // Ensure all data is covered

    // Schedule tasks in the ThreadPool
    for (int i = 0; i < numThreads; ++i) {
        int start = i * chunkSize;
        int end = min(start + chunkSize, n);
        if (start < n) {
            // schedule the task with lambda function
            pool.schedule([start, end, i, &data, &results]() {computeSum(data, start, end, &results[i]);});
            //pool.schedule(bind(computeSum, ref(data), start, end, &results[i]));
        }
    }

    // Wait for all threads to finish
    pool.wait();

    // Calculate total sum
    int totalSum = accumulate(results.begin(), results.end(), 0);
    cout << "Total sum of elements: " << totalSum << endl;

    int res = tpcustomtest(2, (char*[]){"tpcustomtest", "--all"});
    return res;
    
    return 0;
}

