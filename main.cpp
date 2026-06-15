#include "utils.h"
#include "database.h"
#include "session.h"
#include "router.h"
#include "handlers.h"
#include <sys/socket.h>
#include "threadpool.h"
#include <netinet/in.h>
#include <unistd.h>
#include <thread>
#include <iostream>
#include <atomic>
#include <csignal>
#include <chrono>

std::atomic<bool> running(true);
std::atomic<int>  active_requests(0);
int server_fd;

void handle_signal(int sig) {
    running = false;
    close(server_fd);
}

void handle_client(int client_fd, AppContext& ctx, Router& router) {
    active_requests++;

    std::string request = read_full_request(client_fd);
    std::string method  = extract_method(request);
    std::string path    = extract_path(request);
    std::string ip      = get_client_ip(client_fd);

    // rate limit check
    if (!ctx.rate_limiter->allow(ip)) {
        std::string response =
            "HTTP/1.1 429 Too Many Requests\r\n"
            "Content-Type: application/json\r\n"
            "Retry-After: 60\r\n\r\n"
            "{\"error\":\"Too many requests, slow down!\"}";
        send(client_fd, response.c_str(), response.size(), 0);
        close(client_fd);
        active_requests--;
        return;
    }

    log_request(ip, method, path, 200);
    router.dispatch(request, method, path, client_fd);
    close(client_fd);

    active_requests--;
}

int main() {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    AppContext ctx {
        std::make_shared<Database>("database.db"),
        std::make_shared<SessionManager>(),
        std::make_shared<FileCache>(),
        std::make_shared<RateLimiter>(100, 60)  // 100 requests per 60 seconds
    };

    Router router(ctx);
    router.add("GET",  "/login.html",    handle_login_page);
    router.add("POST", "/login",         handle_login_post);
    router.add("POST", "/api/login", handle_api_login);
    router.add("GET",  "/logout",        handle_logout);
    router.add("POST", "/register", handle_register_post);
    router.add("POST", "/upload", handle_upload_post);
    router.add("GET", "/dashboard.html", require_auth(handle_dashboard));
    router.add("GET", "/api/dashboard", require_auth(handle_api_dashboard));
    router.add("GET", "/api/applications", require_auth(handle_api_applications));
    router.add("GET", "/health", handle_health);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in address{};
    address.sin_family      = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port        = htons(8080);
    if (bind(server_fd, (sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        return 1;
    }
    if (listen(server_fd, 10) < 0) {
        perror("listen failed");
        return 1;
    }

    std::cout << "Server running on http://localhost:8080\n";
    std::cout.flush();

    ThreadPool pool(10);  // 10 worker threads

    while (running) {
        int client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd < 0) break;

        pool.submit([client_fd, &ctx, &router]() {
            handle_client(client_fd, ctx, router);
        });
    }

    std::cout << "\nShutting down... waiting for " << active_requests << " active request(s)\n";
    std::cout.flush();

    auto start = std::chrono::steady_clock::now();
    const int TIMEOUT_SECONDS = 10;

    while (active_requests > 0) {
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed > std::chrono::seconds(TIMEOUT_SECONDS)) {
            std::cout << "Timeout — forcing shutdown with " 
                      << active_requests << " request(s) still active\n";
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "Shutdown complete ✅\n";
    return 0;
}
