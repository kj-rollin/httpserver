#pragma once
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
    return end == std::string::npos
           ? request.substr(start, request.find("\r\n", start) - start)
           : request.substr(start, end - start);
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

// serve static file
void serve_file(const std::string& path, int client_fd) {
    std::string full_path = 
        std::filesystem::current_path().string() + "/www" + path;
    std::string content = read_file(full_path);
    if (!content.empty()) {
        std::string response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: " + detect_content_type(path) + 
            "; charset=utf-8\r\n\r\n" + content;
        send(client_fd, response.c_str(), response.size(), 0);
    } else {
        std::string not_found =
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/html\r\n\r\n"
            "<h1>404 Not Found</h1>";
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
    std::cout << line << "\n";
    std::ofstream log_file("server.log", std::ios::app);
    if (log_file.is_open()) log_file << line << "\n";
}
