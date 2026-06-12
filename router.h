#pragma once
#include "utils.h"
#include "cache.h"
#include "database.h"
#include "session.h"
#include <functional>
#include <map>
#include <string>
#include <memory>

// forward declaration — defined in handlers.h

struct AppContext {
    std::shared_ptr<Database>       db;
    std::shared_ptr<SessionManager> sessions;
    std::shared_ptr<FileCache>       cache;
};

// defined in handlers.h
void handle_api_delete_application(const std::string& request, int client_fd, AppContext& ctx, const std::string& path);

using Handler = std::function<void(
    const std::string&,
    int,
    AppContext&
)>;

// wraps a handler with login check
Handler require_auth(Handler handler) {
    return [handler](const std::string& request, int client_fd, AppContext& ctx) {
        std::string token    = extract_cookie(request, "session");
        std::string username = ctx.sessions->validate(token);

        if (username.empty()) {
            std::string response =
                "HTTP/1.1 302 Found\r\nLocation: /login.html\r\n\r\n";
            send(client_fd, response.c_str(), response.size(), 0);
            return;
        }

        handler(request, client_fd, ctx);
    };
}

class Router {
private:
    std::map<std::string, Handler> routes;
    AppContext& ctx;

    void serve_static(const std::string& path, int client_fd) {
        serve_file_cached(path, client_fd, *ctx.cache);
    }

public:
    Router(AppContext& context) : ctx(context) {}

    void add(const std::string& method,
             const std::string& path,
             Handler handler) {
        routes[method + " " + path] = handler;
    }

    void dispatch(const std::string& request,
                  const std::string& method,
                  const std::string& path,
                  int client_fd) {
        // dynamic route — DELETE /api/applications/<id>
        if (method == "DELETE" && path.find("/api/applications/") == 0) {
            handle_api_delete_application(request, client_fd, ctx, path);
            return;
        }

        std::string key = method + " " + path;
        auto it = routes.find(key);
        if (it != routes.end()) {
            it->second(request, client_fd, ctx);
        } else {
            serve_static(path, client_fd);
        }
    }
};
