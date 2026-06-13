// handlers.h
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
            "HTTP/1.1 302 Found\r\n"
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
            "HTTP/1.1 302 Found\r\n"
            "Set-Cookie: session=" + token + "; HttpOnly; Path=/\r\n"
            "Location: /dashboard.html\r\n"
            "\r\n";
        send(client_fd, response.c_str(), response.size(), 0);
    } else {
        std::string response =
            "HTTP/1.1 302 Found\r\n"
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
            "HTTP/1.1 302 Found\r\n"
            "Location: /login.html\r\n\r\n";
        send(client_fd, response.c_str(), response.size(), 0);
        return;
    }

    // verify CSRF token
    std::string body_preview = extract_body(request);
    size_t csrf_pos = body_preview.find("name=\"csrf_token\"");
    std::string submitted_csrf;
    if (csrf_pos != std::string::npos) {
        size_t value_start = body_preview.find("\r\n\r\n", csrf_pos) + 4;
        size_t value_end   = body_preview.find("\r\n", value_start);
        submitted_csrf = body_preview.substr(value_start, value_end - value_start);
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

    for (auto& file : files) {
        std::string unique_name = generate_token() + "_" + file.filename;
        std::string filepath    = uploads_dir + "/" + unique_name;

        std::ofstream out(filepath, std::ios::binary);
        out.write(file.data.c_str(), file.data.size());
        out.close();

        // 4. save record to database
        ctx.db->save_application(username, file.filename, 
                                 "uploads/" + unique_name);
    }

    std::string response_body = 
        "<h1>Application submitted! ✅</h1>"
        "<p>Files uploaded: " + std::to_string(files.size()) + "</p>";

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
        "HTTP/1.1 302 Found\r\n"
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
            "HTTP/1.1 302 Found\r\n"
            "Location: /register.html?error=1\r\n"
            "\r\n";
        send(client_fd, response.c_str(), response.size(), 0);
        return;
    }

    if (ctx.db->register_user(username, password)) {
        // registered! redirect to login
        std::string response =
            "HTTP/1.1 302 Found\r\n"
            "Location: /login.html?registered=1\r\n"
            "\r\n";
        send(client_fd, response.c_str(), response.size(), 0);
    } else {
        // username taken
        std::string response =
            "HTTP/1.1 302 Found\r\n"
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
        content.replace(pos, user_placeholder.size(), username);
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

        response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Set-Cookie: session=" + token + "; HttpOnly; Path=/\r\n"
            "\r\n" + json;
    } else {
        std::string json = "{\"success\":false,\"error\":\"Invalid credentials\"}";

        response =
            "HTTP/1.1 401 Unauthorized\r\n"
            "Content-Type: application/json\r\n"
            "\r\n" + json;
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
        std::string response =
            "HTTP/1.1 401 Unauthorized\r\n"
            "Content-Type: application/json\r\n\r\n" + json;
        send(client_fd, response.c_str(), response.size(), 0);
        return;
    }

    std::string csrf = ctx.sessions->get_csrf(token);

    std::string json = 
        "{\"username\":\"" + json_escape(username) + "\","
        "\"csrf_token\":\"" + json_escape(csrf) + "\"}";

    std::string response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n\r\n" + json;

    send(client_fd, response.c_str(), response.size(), 0);
}

void handle_api_applications(const std::string& request,
                             int client_fd,
                             AppContext& ctx) {
    std::string token    = extract_cookie(request, "session");
    std::string username = ctx.sessions->validate(token);

    if (username.empty()) {
        std::string json = "{\"error\":\"Not authenticated\"}";
        std::string response =
            "HTTP/1.1 401 Unauthorized\r\n"
            "Content-Type: application/json\r\n\r\n" + json;
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

    std::string response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n\r\n" + json;

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
        std::string response =
            "HTTP/1.1 401 Unauthorized\r\n"
            "Content-Type: application/json\r\n\r\n" + json;
        send(client_fd, response.c_str(), response.size(), 0);
        return;
    }

    size_t last_slash = path.rfind('/');
    std::string id_str = path.substr(last_slash + 1);

    bool success = ctx.db->delete_application(username, id_str);

    std::string json = success 
        ? "{\"success\":true}" 
        : "{\"success\":false,\"error\":\"Not found or not yours\"}";

    std::string response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n\r\n" + json;

    send(client_fd, response.c_str(), response.size(), 0);
}

void handle_health(const std::string& request,
                   int client_fd,
                   AppContext& ctx) {
    std::string json = "{\"status\":\"ok\"}";

    std::string response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n\r\n" + json;

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
        std::string response =
            "HTTP/1.1 403 Forbidden\r\n"
            "Content-Type: application/json\r\n\r\n" + json;
        send(client_fd, response.c_str(), response.size(), 0);
        return;
    }

    size_t last_slash = path.rfind('/');
    std::string id = path.substr(last_slash + 1);

    std::string body   = extract_body(request);
    std::string status = extract_json_field(body, "status");

    if (status.empty()) {
        std::string json = "{\"error\":\"Missing status field\"}";
        std::string response =
            "HTTP/1.1 400 Bad Request\r\n"
            "Content-Type: application/json\r\n\r\n" + json;
        send(client_fd, response.c_str(), response.size(), 0);
        return;
    }

    bool success = ctx.db->update_application_status(id, status);

    std::string json = success
        ? "{\"success\":true,\"status\":\"" + json_escape(status) + "\"}"
        : "{\"success\":false,\"error\":\"Application not found\"}";

    std::string response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n\r\n" + json;

    send(client_fd, response.c_str(), response.size(), 0);
}
