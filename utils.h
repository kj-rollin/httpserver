#pragma once
#include "cache.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string>
#include <sstream>
#include <fstream>
#include <map>
#include <algorithm>
#include <filesystem>
#include <ctime>
#include <iostream>

// extract path from request
std::string extract_path(const std::string& request) {
    std::istringstream stream(request);
    std::string method, path;
    stream >> method >> path;
    
    // strip query string
    size_t query_pos = path.find('?');
    if (query_pos != std::string::npos)
        path = path.substr(0, query_pos);
    if (path == "/") path = "/index.html";
    return path;
}

// extract method
std::string extract_method(const std::string& request) {
    std::istringstream stream(request);
    std::string method;
    stream >> method;
    return method;
}

// extract body
std::string extract_body(const std::string& request) {
    size_t pos = request.find("\r\n\r\n");
    if (pos == std::string::npos) return "";
    return request.substr(pos + 4);
}

// extract cookie
std::string extract_cookie(const std::string& request,
                           const std::string& name) {
    std::string search = name + "=";
    size_t pos = request.find(search);
    if (pos == std::string::npos) return "";
    size_t start = pos + search.size();
    size_t end   = request.find(';', start);
    if (end == std::string::npos) {
        size_t line_end = request.find("\r\n", start);
        if (line_end == std::string::npos) return "";
        return request.substr(start, line_end - start);
    }
    return request.substr(start, end - start);
}

std::string extract_boundary(const std::string& request) {
    size_t pos = request.find("boundary=");
    if (pos == std::string::npos) return "";
    
    // Start extracting after "boundary="
    size_t start = pos + 9;  // length of "boundary=" is 9 characters
    
    // Find where the boundary value ends (at \r\n or end of string)
    size_t end = request.find("\r\n", start);
    
    // Extract everything from start to end (or to end of string if no \r\n found)
    return request.substr(start, end - start);
}

// parse body into map
std::map<std::string, std::string> parse_body(const std::string& body) {
    std::map<std::string, std::string> result;
    std::istringstream stream(body);
    std::string piece;
    while (std::getline(stream, piece, '&')) {
        size_t pos = piece.find('=');
        if (pos != std::string::npos)
            result[piece.substr(0, pos)] = piece.substr(pos + 1);
    }
    return result;
}

// get client IP
std::string get_client_ip(int client_fd) {
    sockaddr_in addr{};
    socklen_t len = sizeof(addr);
    getpeername(client_fd, (sockaddr*)&addr, &len);
    return inet_ntoa(addr.sin_addr);
}

// detect content type
std::string detect_content_type(const std::string& path) {
    std::filesystem::path p(path);
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    static const std::map<std::string, std::string> mime_map = {
        {".html", "text/html"},
        {".css",  "text/css"},
        {".js",   "application/javascript"},
        {".png",  "image/png"},
        {".json", "application/json"},
        {".mp4",  "video/mp4"},
        {".mp3",  "audio/mpeg"}
    };
    auto it = mime_map.find(ext);
    return it != mime_map.end() ? it->second : "application/octet-stream";
}

// read file
std::string read_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return "";
    return std::string(
        std::istreambuf_iterator<char>(file), {});
}

// build 404 response — uses custom www/404.html if it exists
std::string build_404_response() {
    std::string custom_path = std::filesystem::current_path().string() + "/www/404.html";
    std::string custom_content = read_file(custom_path);

    std::string body = !custom_content.empty() 
        ? custom_content 
        : "<h1>404 Not Found</h1>";

    return "HTTP/1.1 404 Not Found\r\nConnection: close\r\n"
           "Content-Type: text/html; charset=utf-8\r\n\r\n" + body;
}


// safely resolve a requested path within www/, blocking traversal
std::string resolve_safe_path(const std::string& path) {
    namespace fs = std::filesystem;
    fs::path base_dir = fs::current_path() / "www";
    if (!fs::exists(base_dir)) return "";
    fs::path base_canonical = fs::canonical(base_dir);
    fs::path requested = fs::weakly_canonical(base_dir / fs::path(path).relative_path());
    std::string base_str = base_canonical.string();
    std::string req_str  = requested.string();
    if (req_str.compare(0, base_str.size(), base_str) != 0) return "";
    return req_str;
}
// serve static file
void serve_file(const std::string& path, int client_fd) {
    std::string full_path = resolve_safe_path(path);
    if (full_path.empty()) {
        std::string not_found = build_404_response();
        send(client_fd, not_found.c_str(), not_found.size(), 0);
        return;
    }
    std::string content = read_file(full_path);
    if (!content.empty()) {
        std::string response =
            "HTTP/1.1 200 OK\r\nConnection: close\r\n"
            "Content-Type: " + detect_content_type(path) + 
            "; charset=utf-8\r\n\r\n" + content;
        send(client_fd, response.c_str(), response.size(), 0);
    } else {
        std::string not_found = build_404_response();
        send(client_fd, not_found.c_str(), not_found.size(), 0);
    }
}

// log request
void log_request(const std::string& ip,
                 const std::string& method,
                 const std::string& path,
                 int status_code) {
    std::time_t now = std::time(nullptr);
    char time_buf[20];
    std::strftime(time_buf, sizeof(time_buf), 
                  "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    std::string line = "[" + std::string(time_buf) + "] " +
                       ip + " " + method + " " + path + " " +
                       std::to_string(status_code);
    std::cerr << line << "\n";
    std::cerr.flush();
    std::ofstream log_file("server.log", std::ios::app);
    if (log_file.is_open()) log_file << line << "\n";
}

std::string read_full_request(int client_fd) {
    std::string request;
    char buffer[4096];

    // step 1 — read until we have headers (find \r\n\r\n)
    size_t header_end;
    while ((header_end = request.find("\r\n\r\n")) == std::string::npos) {
        int bytes = recv(client_fd, buffer, sizeof(buffer), 0);
        if (bytes <= 0) return request;  // connection closed
        request.append(buffer, bytes);
    }

    // step 2 — extract Content-Length
    size_t cl_pos = request.find("Content-Length: ");
    long content_length = 0;
    if (cl_pos != std::string::npos) {
        size_t cl_start = cl_pos + 16;
        size_t cl_end   = request.find("\r\n", cl_start);
        if (cl_end != std::string::npos) {
            try {
                content_length = std::stol(request.substr(cl_start, cl_end - cl_start));
            } catch (...) {
            }
        }
    }

    const long MAX_REQUEST_SIZE = 10 * 1024 * 1024;
    if (content_length > MAX_REQUEST_SIZE || content_length < 0) {
        return "";
    }

    // step 3 — keep reading until we have content_length bytes of body
    size_t body_start = header_end + 4;
    size_t body_received = request.size() - body_start;

    while ((long)body_received < content_length) {
        int bytes = recv(client_fd, buffer, sizeof(buffer), 0);
        if (bytes <= 0) break;
        request.append(buffer, bytes);
        body_received += bytes;
    }

    return request;
}

// cached version — for static files (CSS/JS/images)
void serve_file_cached(const std::string& path, int client_fd, FileCache& cache, const std::string& request) {
    std::string full_path = resolve_safe_path(path);
    if (full_path.empty()) {
        std::string not_found = build_404_response();
        send(client_fd, not_found.c_str(), not_found.size(), 0);
        return;
    }

    bool accepts_gzip = request.find("Accept-Encoding:") != std::string::npos &&
                         request.find("gzip") != std::string::npos;

    auto [content, is_compressed] = cache.get(full_path, accepts_gzip);

    if (!content.empty()) {
        std::string encoding_header = is_compressed ? "Content-Encoding: gzip\r\n" : "";
        std::string response =
            "HTTP/1.1 200 OK\r\nConnection: close\r\n"
            "Content-Type: " + detect_content_type(path) +
            "; charset=utf-8\r\n" + encoding_header + "\r\n";
        send(client_fd, response.c_str(), response.size(), 0);
        send(client_fd, content.c_str(), content.size(), 0);
    } else {
        std::string not_found = build_404_response();
        send(client_fd, not_found.c_str(), not_found.size(), 0);
    }
}

// build complete HTTP response with Content-Length
std::string build_response(int status_code,
                           const std::string& status_text,
                           const std::string& content_type,
                           const std::string& body,
                           const std::string& extra_headers = "") {
    return "HTTP/1.1 " + std::to_string(status_code) + " " + status_text + "\r\n"
           "Content-Type: " + content_type + "\r\n"
           "Content-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\n" +
           extra_headers +
           "\r\n" + body;
}

// shorthand for JSON responses
std::string json_response(int status_code,
                          const std::string& status_text,
                          const std::string& json_body,
                          const std::string& extra_headers = "") {
    return build_response(status_code, status_text,
                          "application/json", json_body, extra_headers);
}
