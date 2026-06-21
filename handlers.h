// handlers.h
#include <set>
#include <algorithm>
#include "json.h"
#pragma once
#include "router.h"
#include "utils.h"
#include "multipart.h"
#include "crypto.h"


// ─── login page ───────────────────────────
void handle_login_page(const std::string& request,
                       int client_fd,
                       AppContext& ctx) {
    // check if already logged in
    std::string token    = extract_cookie(request, "session");
    std::string username = ctx.sessions->validate(token);

    if (!username.empty()) {
        // already logged in → dashboard
        std::string response =
            "HTTP/1.1 302 Found\r\nConnection: close\r\n"
            "Location: /dashboard.html\r\n"
            "\r\n";
        send(client_fd, response.c_str(), response.size(), 0);
        return;
    }
    // serve login form
    serve_file("/login.html", client_fd);
}

// ─── login POST ───────────────────────────
void handle_login_post(const std::string& request,
                       int client_fd,
                       AppContext& ctx) {
    auto params   = parse_body(extract_body(request));
    auto username = params["username"];
    auto password = params["password"];

    if (ctx.db->check_login(username, password)) {
        std::string token = ctx.sessions->create(username);
        std::string response =
            "HTTP/1.1 302 Found\r\nConnection: close\r\n"
            "Set-Cookie: session=" + token + "; HttpOnly; Path=/\r\n"
            "Location: /dashboard.html\r\n"
            "\r\n";
        send(client_fd, response.c_str(), response.size(), 0);
    } else {
        std::string response =
            "HTTP/1.1 302 Found\r\nConnection: close\r\n"
            "Location: /login.html?error=1\r\n"
            "\r\n";
        send(client_fd, response.c_str(), response.size(), 0);
    }
}

void handle_upload_post(const std::string& request,
                        int client_fd,
                        AppContext& ctx) {
    // 1. check if logged in
    std::string token    = extract_cookie(request, "session");
    std::string username = ctx.sessions->validate(token);

    if (username.empty()) {
        std::string response =
            "HTTP/1.1 302 Found\r\nConnection: close\r\n"
            "Location: /login.html\r\n\r\n";
        send(client_fd, response.c_str(), response.size(), 0);
        return;
    }

    // verify CSRF token
    std::string body_preview = extract_body(request);
    size_t csrf_pos = body_preview.find("name=\"csrf_token\"");
    std::string submitted_csrf;
    if (csrf_pos != std::string::npos) {
        size_t header_end = body_preview.find("\r\n\r\n", csrf_pos);
        if (header_end != std::string::npos) {
            size_t value_start = header_end + 4;
            size_t value_end   = body_preview.find("\r\n", value_start);
            if (value_end != std::string::npos) {
                submitted_csrf = body_preview.substr(value_start, value_end - value_start);
            }
        }
    }

    if (!ctx.sessions->verify_csrf(token, submitted_csrf)) {
        std::string response =
            "HTTP/1.1 403 Forbidden\r\n\r\n<h1>CSRF token invalid</h1>";
        send(client_fd, response.c_str(), response.size(), 0);
        return;
    }

    // 2. parse multipart
    std::string boundary = extract_boundary(request);
    std::string body     = extract_body(request);
    auto files = parse_multipart(body, boundary);

    if (files.empty()) {
        std::string response =
            "HTTP/1.1 400 Bad Request\r\n\r\n<h1>No file uploaded</h1>";
        send(client_fd, response.c_str(), response.size(), 0);
        return;
    }

    // 3. save each file with unique name
    std::string uploads_dir = 
        std::filesystem::current_path().string() + "/uploads";
    std::filesystem::create_directory(uploads_dir);
    const long MAX_FILE_SIZE = 5 * 1024 * 1024;  // 5MB
    std::set<std::string> allowed_extensions = {".pdf", ".png", ".jpg", ".jpeg", ".doc", ".docx"};
    int saved_count = 0;
    int rejected_count = 0;

    for (auto& file : files) {
        // sanitize filename
        std::string safe_filename = file.filename;
        safe_filename.erase(
            std::remove_if(safe_filename.begin(), safe_filename.end(),
                [](char c) { return c == 47 || c == 92 || c == 0; }),
            safe_filename.end());
        if (safe_filename.empty()) safe_filename = "unnamed";

        // validate size
        if ((long)file.data.size() > MAX_FILE_SIZE) {
            rejected_count++;
            continue;
        }

        // validate extension
        std::filesystem::path p(safe_filename);
        std::string ext = p.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (allowed_extensions.find(ext) == allowed_extensions.end()) {
            rejected_count++;
            continue;
        }

        std::string unique_name = generate_token() + "_" + safe_filename;
        std::string filepath    = uploads_dir + "/" + unique_name;

        std::ofstream out(filepath, std::ios::binary);
        if (!out) {
            rejected_count++;
            continue;
        }
        out.write(file.data.c_str(), file.data.size());
        out.close();

        ctx.db->save_application(username, safe_filename,
                                 "uploads/" + unique_name);
        saved_count++;
    }

    if (saved_count > 0) {
        // TODO: replace with notify_all(get_admin_usernames(), ...) — see ws_registry notes
        std::string notif = "{\"type\":\"new_application\",\"applicant\":\"" + json_escape(username) + "\"}";
        ctx.ws_registry->notify("admin", notif);
    }

    std::string response_body = 
        "<h1>Application submitted! ✅</h1>"
        "<p>Files saved: " + std::to_string(saved_count) + "</p>"
        "<p>Files rejected: " + std::to_string(rejected_count) + "</p>";

    std::string response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n\r\n" + response_body;

    send(client_fd, response.c_str(), response.size(), 0);
}
// ─── logout ───────────────────────────────
void handle_logout(const std::string& request,
                   int client_fd,
                   AppContext& ctx) {
    std::string token = extract_cookie(request, "session");
    if (!token.empty()) ctx.sessions->remove(token);

    std::string response =
        "HTTP/1.1 302 Found\r\nConnection: close\r\n"
        "Set-Cookie: session=; "
        "expires=Thu, 01 Jan 1970 00:00:00 GMT; Path=/\r\n"
        "Location: /login.html\r\n"
        "\r\n";
    send(client_fd, response.c_str(), response.size(), 0);
}

void handle_register_post(const std::string& request,
                          int client_fd,
                          AppContext& ctx) {
    auto params   = parse_body(extract_body(request));
    auto username = params["username"];
    auto password = params["password"];

    if (username.empty() || password.empty()) {
        std::string response =
            "HTTP/1.1 302 Found\r\nConnection: close\r\n"
            "Location: /register.html?error=1\r\n"
            "\r\n";
        send(client_fd, response.c_str(), response.size(), 0);
        return;
    }

    if (ctx.db->register_user(username, password)) {
        // registered! redirect to login
        std::string response =
            "HTTP/1.1 302 Found\r\nConnection: close\r\n"
            "Location: /login.html?registered=1\r\n"
            "\r\n";
        send(client_fd, response.c_str(), response.size(), 0);
    } else {
        // username taken
        std::string response =
            "HTTP/1.1 302 Found\r\nConnection: close\r\n"
            "Location: /register.html?error=2\r\n"
            "\r\n";
        send(client_fd, response.c_str(), response.size(), 0);
    }
}

void handle_upload_page(const std::string& request,
                        int client_fd,
                        AppContext& ctx) {
    std::string token = extract_cookie(request, "session");
    std::string csrf  = ctx.sessions->get_csrf(token);

    std::string full_path = std::filesystem::current_path().string() + "/www/upload.html";
    std::string content = read_file(full_path);

    // replace placeholder
    size_t pos = content.find("{{CSRF_TOKEN}}");
    if (pos != std::string::npos) {
        content.replace(pos, 14, csrf);  // 14 = length of "{{CSRF_TOKEN}}"
    }

    std::string response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "\r\n" + content;

    send(client_fd, response.c_str(), response.size(), 0);
}

void handle_dashboard(const std::string& request,
                      int client_fd,
                      AppContext& ctx) {
    std::string token    = extract_cookie(request, "session");
    std::string username = ctx.sessions->validate(token);
    std::string csrf     = ctx.sessions->get_csrf(token);

    std::string full_path = std::filesystem::current_path().string() + "/www/dashboard.html";
    std::string content = read_file(full_path);

    // replace placeholders
    size_t pos;
    std::string user_placeholder = "{{USERNAME}}";
    if ((pos = content.find(user_placeholder)) != std::string::npos) {
        content.replace(pos, user_placeholder.size(), html_escape(username));
    }

    std::string csrf_placeholder = "{{CSRF_TOKEN}}";
    if ((pos = content.find(csrf_placeholder)) != std::string::npos) {
        content.replace(pos, csrf_placeholder.size(), csrf);
    }

    std::string response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "\r\n" + content;

    send(client_fd, response.c_str(), response.size(), 0);
}

void handle_api_login(const std::string& request,
                      int client_fd,
                      AppContext& ctx) {
    auto params   = parse_body(extract_body(request));
    auto username = params["username"];
    auto password = params["password"];

    std::string response;

    if (ctx.db->check_login(username, password)) {
        std::string token = ctx.sessions->create(username);
        std::string csrf  = ctx.sessions->get_csrf(token);

        std::string json = 
            "{\"success\":true,"
            "\"username\":\"" + json_escape(username) + "\","
            "\"csrf_token\":\"" + json_escape(csrf) + "\"}";

        response = json_response(200, "OK", json,
            "Set-Cookie: session=" + token + "; HttpOnly; Path=/\r\n");
    } else {
        std::string json = "{\"success\":false,\"error\":\"Invalid credentials\"}";
        response = json_response(401, "Unauthorized", json);
    }

    send(client_fd, response.c_str(), response.size(), 0);
}

void handle_api_dashboard(const std::string& request,
                          int client_fd,
                          AppContext& ctx) {
    std::string token    = extract_cookie(request, "session");
    std::string username = ctx.sessions->validate(token);

    if (username.empty()) {
        std::string json = "{\"error\":\"Not authenticated\"}";
        std::string response = json_response(401, "Unauthorized", json);
        send(client_fd, response.c_str(), response.size(), 0);
        return;
    }

    std::string csrf = ctx.sessions->get_csrf(token);

    std::string json = 
        "{\"username\":\"" + json_escape(username) + "\","
        "\"csrf_token\":\"" + json_escape(csrf) + "\"}";

    std::string response = json_response(200, "OK", json);

    send(client_fd, response.c_str(), response.size(), 0);
}

void handle_api_applications(const std::string& request,
                             int client_fd,
                             AppContext& ctx) {
    std::string token    = extract_cookie(request, "session");
    std::string username = ctx.sessions->validate(token);

    if (username.empty()) {
        std::string json = "{\"error\":\"Not authenticated\"}";
        std::string response = json_response(401, "Unauthorized", json);
        send(client_fd, response.c_str(), response.size(), 0);
        return;
    }

    auto apps = ctx.db->get_applications(username);

    std::string json = "[";
    for (size_t i = 0; i < apps.size(); i++) {
        if (i > 0) json += ",";
        json += "{\"id\":" + apps[i]["id"] + ","
                "\"filename\":\"" + json_escape(apps[i]["filename"]) + "\","
                "\"upload_time\":\"" + json_escape(apps[i]["upload_time"]) + "\","
                "\"status\":\"" + json_escape(apps[i]["status"]) + "\"}";
    }
    json += "]";

    std::string response = json_response(200, "OK", json);

    send(client_fd, response.c_str(), response.size(), 0);
}

void handle_api_delete_application(const std::string& request,
                                   int client_fd,
                                   AppContext& ctx,
                                   const std::string& path) {
    std::string token    = extract_cookie(request, "session");
    std::string username = ctx.sessions->validate(token);

    if (username.empty()) {
        std::string json = "{\"error\":\"Not authenticated\"}";
        std::string response = json_response(401, "Unauthorized", json);
        send(client_fd, response.c_str(), response.size(), 0);
        return;
    }

    size_t last_slash = path.rfind('/');
    std::string id_str = path.substr(last_slash + 1);

    bool success = ctx.db->delete_application(username, id_str);

    std::string json = success 
        ? "{\"success\":true}" 
        : "{\"success\":false,\"error\":\"Not found or not yours\"}";

    std::string response = json_response(200, "OK", json);

    send(client_fd, response.c_str(), response.size(), 0);
}

void handle_health(const std::string& request,
                   int client_fd,
                   AppContext& ctx) {
    std::string json = "{\"status\":\"ok\"}";

    std::string response = json_response(200, "OK", json);

    send(client_fd, response.c_str(), response.size(), 0);
}

void handle_api_update_application(const std::string& request,
                                    int client_fd,
                                    AppContext& ctx,
                                    const std::string& path) {
    std::string token    = extract_cookie(request, "session");
    std::string username = ctx.sessions->validate(token);

    if (username.empty() || !ctx.db->is_admin(username)) {
        std::string json = "{\"error\":\"Admin access required\"}";
        std::string response = json_response(403, "Forbidden", json);
        send(client_fd, response.c_str(), response.size(), 0);
        return;
    }

    size_t last_slash = path.rfind('/');
    std::string id = path.substr(last_slash + 1);

    std::string body   = extract_body(request);
    std::string status = extract_json_field(body, "status");

    // whitelist valid statuses
    std::set<std::string> valid_statuses = {"pending", "reviewed", "accepted", "rejected"};
    if (status.empty() || valid_statuses.find(status) == valid_statuses.end()) {
        std::string json = "{\"error\":\"Invalid or missing status field\"}";
        std::string response = json_response(400, "Bad Request", json);
        send(client_fd, response.c_str(), response.size(), 0);
        return;
    }

    bool success = ctx.db->update_application_status(id, status);

    std::string json = success
        ? "{\"success\":true,\"status\":\"" + json_escape(status) + "\"}"
        : "{\"success\":false,\"error\":\"Application not found\"}";

    std::string response = json_response(200, "OK", json);

    send(client_fd, response.c_str(), response.size(), 0);
}
