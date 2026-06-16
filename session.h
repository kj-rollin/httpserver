// session.h
#pragma once
#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <random>
#include <sstream>
#include <iomanip>
#include <ctime>

struct Session {
    std::string username;
    std::time_t last_seen;
    std::string csrf_token;
};

class SessionManager {
private:
    std::map<std::string, Session> sessions;
    std::mutex sessions_mutex;
    const int EXPIRY_MINUTES = 30;
    std::atomic<int> validate_count{0};

    // remove expired sessions — caller must already hold sessions_mutex!
    void cleanup_expired() {
        for (auto it = sessions.begin(); it != sessions.end(); ) {
            if (is_expired(it->second)) {
                it = sessions.erase(it);
            } else {
                ++it;
            }
        }
    }

    // generate random token
    std::string generate_token() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 15);
        
        const char* hex_chars = "0123456789abcdef";
        std::string token;
        token.reserve(64);
        
        for (int i = 0; i < 64; ++i) {
            token += hex_chars[dis(gen)];
        }
        return token;
    }

    // check if session is expired
    bool is_expired(const Session& s) {
        std::time_t now = std::time(nullptr);
        double diff_seconds = std::difftime(now, s.last_seen);
        return diff_seconds > (EXPIRY_MINUTES * 60);
    }

public:
    // create new session for user — returns token
    std::string create(const std::string& username) {
        std::lock_guard<std::mutex> lock(sessions_mutex);
        
        std::string token = generate_token();
        std::string csrf = generate_token();
        Session session{username, std::time(nullptr), csrf};
        sessions[token] = session;
        
        return token;
    }

    // validate token — returns username or empty string
    std::string validate(const std::string& token) {
        std::lock_guard<std::mutex> lock(sessions_mutex);

        if (++validate_count >= 100) {
            validate_count = 0;
            cleanup_expired();
        }
        
        auto it = sessions.find(token);
        if (it == sessions.end()) {
            return "";  // token not found
        }
        
        if (is_expired(it->second)) {
            sessions.erase(it);  // cleanup expired session
            return "";
        }
        
        // update last_seen
        it->second.last_seen = std::time(nullptr);
        return it->second.username;
    }

    // get CSRF token for a session
    std::string get_csrf(const std::string& token) {
        std::lock_guard<std::mutex> lock(sessions_mutex);
        auto it = sessions.find(token);
        if (it == sessions.end()) return "";
        return it->second.csrf_token;
    }

    // verify CSRF token matches
    bool verify_csrf(const std::string& session_token, const std::string& csrf) {
        std::lock_guard<std::mutex> lock(sessions_mutex);
        auto it = sessions.find(session_token);
        if (it == sessions.end()) return false;
        return it->second.csrf_token == csrf;
    }

    // delete session on logout
    void remove(const std::string& token) {
        std::lock_guard<std::mutex> lock(sessions_mutex);
        sessions.erase(token);
    }
};