// database.h
#pragma once          // ← prevents double inclusion
#include <sqlite3.h>
#include <mutex>
#include <string>
#include <stdexcept>

class Database {
   private:
    sqlite3* db;
    std::mutex db_mutex;

    static int login_callback(void* found, int, char** values, char**) {
        *static_cast<bool*>(found) = true;
        return 0;
    }

   public:
    Database(const std::string& path) {
        int rc = sqlite3_open(path.c_str(), &db);
        if (rc != SQLITE_OK) {
            std::string error = sqlite3_errmsg(db);
            sqlite3_close(db);
            throw std::runtime_error("Failed to open database: " + error);
        }
    }

    ~Database() {
        if (db) sqlite3_close(db);
    }

    bool check_login(const std::string& username,
                     const std::string& password) {
        std::lock_guard<std::mutex> lock(db_mutex);
        std::string sql = "SELECT 1 FROM users WHERE username = ? AND password = ? LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            sqlite3_finalize(stmt);
            return false;
        }
        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_STATIC);
        bool found = (sqlite3_step(stmt) == SQLITE_ROW);
        sqlite3_finalize(stmt);
        return found;
    }
};