// handlers.h
#pragma once
#include "router.h"
#include "utils.h"
#include "multipart.h"


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
    // get boundary from header
    std::string boundary = extract_boundary(request);
    if (boundary.empty()) {
        std::string response = 
            "HTTP/1.1 400 Bad Request\r\n\r\n<h1>No boundary found</h1>";
        send(client_fd, response.c_str(), response.size(), 0);
        return;
    }

    // get body
    std::string body = extract_body(request);

    // parse files
    auto files = parse_multipart(body, boundary);

    // save each file
    std::string uploads_dir = 
        std::filesystem::current_path().string() + "/uploads";
    std::filesystem::create_directory(uploads_dir);

    for (auto& file : files) {
        std::string path = uploads_dir + "/" + file.filename;
        std::ofstream out(path, std::ios::binary);
        out.write(file.data.c_str(), file.data.size());
        out.close();
    }

    std::string response_body = 
        "<h1>Uploaded " + std::to_string(files.size()) + " file(s)!</h1>";

    std::string response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "\r\n" + response_body;

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
