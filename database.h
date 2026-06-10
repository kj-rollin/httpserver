// database.h
#pragma once          // ← prevents double inclusion
#include <sqlite3.h>
#include <mutex>
#include <string>
#include <stdexcept>
#include "crypto.h"

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

    
    bool register_user(const std::string& username,
                       const std::string& password) {
        std::lock_guard<std::mutex> lock(db_mutex);

        // hash password before storing!
        std::string hashed = make_password_hash(password);

        std::string sql = 
            "INSERT INTO users (username, password) VALUES (?, ?);";
        sqlite3_stmt* stmt = nullptr;

        int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            sqlite3_finalize(stmt);
            return false;
        }

        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, hashed.c_str(),   -1, SQLITE_STATIC);

        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return rc == SQLITE_DONE;
    }

    // updated check_login — verifies hash
    bool check_login(const std::string& username,
                     const std::string& password) {
        std::lock_guard<std::mutex> lock(db_mutex);

        // get stored hash from database
        std::string sql = 
            "SELECT password FROM users WHERE username = ?;";
        sqlite3_stmt* stmt = nullptr;

        int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            sqlite3_finalize(stmt);
            return false;
        }

        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);

        bool valid = false;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            // get stored hash
            std::string stored = reinterpret_cast<const char*>(
                sqlite3_column_text(stmt, 0)
            );
            // verify password against stored hash
            valid = verify_password(password, stored);
        }

        sqlite3_finalize(stmt);
        return valid;
    }
};
