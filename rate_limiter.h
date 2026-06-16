#pragma once
#include <atomic>
#include <unordered_map>
#include <mutex>
#include <string>
#include <ctime>

struct RateEntry {
    int count;
    std::time_t window_start;
};

class RateLimiter {
private:
    std::unordered_map<std::string, RateEntry> requests;
    std::mutex mtx;
    int max_requests;
    int window_seconds;
    std::atomic<int> request_count{0};

    // remove IPs whose window has expired — caller must hold mtx!
    void cleanup_old_entries() {
        std::time_t now = std::time(nullptr);
        for (auto it = requests.begin(); it != requests.end(); ) {
            if (now - it->second.window_start > window_seconds) {
                it = requests.erase(it);
            } else {
                ++it;
            }
        }
    }

public:
    RateLimiter(int max_req = 100, int window_sec = 60)
        : max_requests(max_req), window_seconds(window_sec) {}

    // returns true if request is allowed, false if rate limited
    bool allow(const std::string& ip) {
        std::lock_guard<std::mutex> lock(mtx);

        if (++request_count >= 100) {
            request_count = 0;
            cleanup_old_entries();
        }

        std::time_t now = std::time(nullptr);
        auto it = requests.find(ip);

        if (it == requests.end()) {
            // first request from this IP
            requests[ip] = {1, now};
            return true;
        }

        RateEntry& entry = it->second;

        // check if window has expired
        if (now - entry.window_start > window_seconds) {
            // reset window
            entry.count        = 1;
            entry.window_start = now;
            return true;
        }

        // within window — increment and check
        entry.count++;
        return entry.count <= max_requests;
    }
};
