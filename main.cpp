// main.cpp — correct order
#include "utils.h"
#include "database.h"
#include "session.h"
#include "router.h"
#include "handlers.h"    // ← must be last!
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <thread>
#include <iostream>

void handle_client(int client_fd, AppContext& ctx, Router& router) {
    char buffer[4096] = {0};
    recv(client_fd, buffer, sizeof(buffer), 0);

    std::string request = buffer;
    std::string method  = extract_method(request);
    std::string path    = extract_path(request);
    std::string ip      = get_client_ip(client_fd);

    log_request(ip, method, path, 200);
    router.dispatch(request, method, path, client_fd);
    close(client_fd);
}

int main() {
    // setup shared resources
    AppContext ctx {
        std::make_shared<Database>("database.db"),
        std::make_shared<SessionManager>()
    };

    // register routes
    Router router(ctx);
    router.add("GET",  "/login.html",    handle_login_page);
    router.add("POST", "/login",         handle_login_post);
    router.add("GET",  "/logout",        handle_logout);
    router.add("POST", "/register", handle_register_post);

    // socket setup
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in address{};
    address.sin_family      = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port        = htons(8080);
    bind(server_fd, (sockaddr*)&address, sizeof(address));
    listen(server_fd, 10);

    std::cout << "Server running on http://localhost:8080\n";
    std::cout.flush();

    while (true) {
        int client_fd = accept(server_fd, nullptr, nullptr);
        std::thread([client_fd, &ctx, &router]() {
            handle_client(client_fd, ctx, router);
        }).detach();
    }
}
