#pragma once
#include <map>
#include <shared_mutex>
#include <string>
#include <filesystem>
#include <fstream>

struct CacheEntry {
    std::string content;
    std::filesystem::file_time_type last_modified;
};

class FileCache {
private:
    std::map<std::string, CacheEntry> cache;
    std::shared_mutex cache_mutex;

public:
    std::string get(const std::string& path) {
        if (!std::filesystem::exists(path)) return "";

        auto current_mtime = std::filesystem::last_write_time(path);

        {
            std::shared_lock<std::shared_mutex> lock(cache_mutex);
            auto it = cache.find(path);
            if (it != cache.end() && it->second.last_modified == current_mtime) {
                return it->second.content;
            }
        }

        std::unique_lock<std::shared_mutex> lock(cache_mutex);

        auto it = cache.find(path);
        if (it != cache.end() && it->second.last_modified == current_mtime) {
            return it->second.content;
        }

        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) return "";

        std::string content(
            std::istreambuf_iterator<char>(file), {});

        cache[path] = CacheEntry{content, current_mtime};
        return content;
    }
};
