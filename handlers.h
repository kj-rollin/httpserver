// handlers.h
#pragma once
#include "router.h"
#include "utils.h"


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
