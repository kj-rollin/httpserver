#pragma once
#include <map>
#include "gzip.h"
#include <shared_mutex>
#include <string>
#include <filesystem>
#include <fstream>

struct CacheEntry {
    std::string content;
    std::string compressed;
    std::filesystem::file_time_type last_modified;
};

class FileCache {
private:
    std::map<std::string, CacheEntry> cache;
    std::shared_mutex cache_mutex;

public:
    // returns pair: (data, is_compressed)
    std::pair<std::string, bool> get(const std::string& path, bool accepts_gzip) {
        if (!std::filesystem::exists(path)) return {"", false};

        auto current_mtime = std::filesystem::last_write_time(path);

        {
            std::shared_lock<std::shared_mutex> lock(cache_mutex);
            auto it = cache.find(path);
            if (it != cache.end() && it->second.last_modified == current_mtime) {
                if (accepts_gzip && !it->second.compressed.empty())
                    return {it->second.compressed, true};
                return {it->second.content, false};
            }
        }

        std::unique_lock<std::shared_mutex> lock(cache_mutex);

        auto it = cache.find(path);
        if (it != cache.end() && it->second.last_modified == current_mtime) {
            if (accepts_gzip && !it->second.compressed.empty())
                return {it->second.compressed, true};
            return {it->second.content, false};
        }

        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) return {"", false};

        std::string content(
            std::istreambuf_iterator<char>(file), {});

        std::string compressed;
        if (content.size() > 1024) {  // only compress files > 1KB
            compressed = gzip_compress(content);
        }

        cache[path] = CacheEntry{content, compressed, current_mtime};

        if (accepts_gzip && !compressed.empty())
            return {compressed, true};
        return {content, false};
    }
};
