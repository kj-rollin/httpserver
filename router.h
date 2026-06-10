#pragma once
#include "utils.h"
#include "database.h"
#include "session.h"
#include <functional>
#include <map>
#include <string>
#include <memory>

struct AppContext {
    std::shared_ptr<Database>       db;
    std::shared_ptr<SessionManager> sessions;
};

using Handler = std::function<void(
    const std::string&,
    int,
    AppContext&
)>;

class Router {
private:
    std::map<std::string, Handler> routes;
    AppContext& ctx;

    void serve_static(const std::string& path, int client_fd) {
        serve_file(path, client_fd);
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
        std::string key = method + " " + path;
        auto it = routes.find(key);
        if (it != routes.end()) {
            it->second(request, client_fd, ctx);
        } else {
            serve_static(path, client_fd);
        }
    }
};
