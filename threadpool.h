#pragma once
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

class ThreadPool {
private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> task_queue;
    std::mutex queue_mutex;
    std::condition_variable cv;
    std::atomic<bool> stopping;

public:
    ThreadPool(int num_threads) : stopping(false) {
        for (int i = 0; i < num_threads; i++) {
            workers.emplace_back([this]() {
                worker_loop();
            });
        }
    }

    ~ThreadPool() {
        stopping = true;
        cv.notify_all();  // wake ALL threads so they can exit
        for (auto& t : workers) {
            if (t.joinable()) t.join();
        }
    }

    void submit(std::function<void()> task) {
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            task_queue.push(task);
        }
        cv.notify_one();  // wake ONE sleeping thread
    }

private:
    void worker_loop() {
        while (true) {
            std::function<void()> task;

            {
                std::unique_lock<std::mutex> lock(queue_mutex);

                // sleep until task available or stopping
                cv.wait(lock, [this]() {
                    return !task_queue.empty() || stopping;
                });

                if (stopping && task_queue.empty()) return;

                task = task_queue.front();
                task_queue.pop();
            }

            task();  // execute outside lock!
        }
    }
};
