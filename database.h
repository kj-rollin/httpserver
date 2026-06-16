// database.h
#pragma once          // ← prevents double inclusion
#include <sqlite3.h>
#include <mutex>
#include <string>
#include <stdexcept>
#include "crypto.h"
#include <ctime>
#include <map>

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
    
     bool save_application(const std::string& username,
                      const std::string& filename,
                      const std::string& filepath) {
     std::lock_guard<std::mutex> lock(db_mutex);

    // 1. get current time — same format as log_request
     std::time_t now = std::time(nullptr);
      char time_buf[20];
     std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
      std::string time_str = time_buf;

     // 2-4 — your code is perfect, keep as is!
     sqlite3_stmt* stmt;
      const char* sql = "INSERT INTO job_applications (username, filename, filepath, upload_time) VALUES (?, ?, ?, ?)";
    
      if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
     }

     sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
      sqlite3_bind_text(stmt, 2, filename.c_str(), -1, SQLITE_STATIC);
     sqlite3_bind_text(stmt, 3, filepath.c_str(), -1, SQLITE_STATIC);
     sqlite3_bind_text(stmt, 4, time_str.c_str(), -1, SQLITE_STATIC);

     bool success = (sqlite3_step(stmt) == SQLITE_DONE);
     sqlite3_finalize(stmt);
    
     return success;
     }

    bool delete_application(const std::string& username, 
                            const std::string& id) {
        std::lock_guard<std::mutex> lock(db_mutex);
        
        int application_id;
        try { application_id = std::stoi(id); } catch (...) { return false; }
        
        const char* sql = "DELETE FROM job_applications WHERE id = ? AND username = ?";
        
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return false;
        }
        
        sqlite3_bind_int(stmt, 1, application_id);
        sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_STATIC);
        
        bool success = (sqlite3_step(stmt) == SQLITE_DONE);
        int changes = sqlite3_changes(db);
        sqlite3_finalize(stmt);
        return success && changes > 0;
    }

    std::vector<std::map<std::string, std::string>> 
    get_applications(const std::string& username) {
        std::lock_guard<std::mutex> lock(db_mutex);
        
        std::vector<std::map<std::string, std::string>> results;
        
        std::string sql = "SELECT id, filename, upload_time, status "
                          "FROM job_applications WHERE username = ?;";
        sqlite3_stmt* stmt = nullptr;
        
        sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::map<std::string, std::string> row;
            row["id"]          = std::to_string(sqlite3_column_int(stmt, 0));
            row["filename"]    = (const char*)sqlite3_column_text(stmt, 1);
            row["upload_time"] = (const char*)sqlite3_column_text(stmt, 2);
            row["status"]      = (const char*)sqlite3_column_text(stmt, 3);
            results.push_back(row);
        }
        
        sqlite3_finalize(stmt);
        return results;
    }

    bool update_application_status(const std::string& id, 
                                   const std::string& new_status) {
        std::lock_guard<std::mutex> lock(db_mutex);

        const char* sql = "UPDATE job_applications SET status = ? WHERE id = ?";

        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return false;
        }

        sqlite3_bind_text(stmt, 1, new_status.c_str(), -1, SQLITE_STATIC);
        int app_id;
        try { app_id = std::stoi(id); } catch (...) { sqlite3_finalize(stmt); return false; }
        sqlite3_bind_int(stmt, 2, app_id);
        bool success = (sqlite3_step(stmt) == SQLITE_DONE);
        int changes = sqlite3_changes(db);
        sqlite3_finalize(stmt);
        return success && changes > 0;
    }

    bool is_admin(const std::string& username) {
        std::lock_guard<std::mutex> lock(db_mutex);
        
        const char* sql = "SELECT role FROM users WHERE username = ?";
        
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return false;
        }
        
        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
        
        bool is_admin_result = false;
        
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            std::string role = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (role == "admin") {
                is_admin_result = true;
            }
        }
        
        sqlite3_finalize(stmt);
        return is_admin_result;
    }
};
