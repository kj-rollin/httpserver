#pragma once
#include <openssl/sha.h>
#include <string>
#include <algorithm>
#include <sys/socket.h>
#include <unordered_map>
#include <mutex>


const std::string WS_MAGIC = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

std::string base64_encode(const unsigned char* data, size_t len) {
    static const char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

    std::string output;
    int i = 0;

    while (i + 3 <= (int)len) {
        unsigned char b0 = data[i], b1 = data[i+1], b2 = data[i+2];
        output += table[b0 >> 2];
        output += table[((b0 & 0x03) << 4) | (b1 >> 4)];
        output += table[((b1 & 0x0F) << 2) | (b2 >> 6)];
        output += table[b2 & 0x3F];
        i += 3;
    }

    int remaining = len - i;
    if (remaining == 1) {
        unsigned char b0 = data[i];
        output += table[b0 >> 2];
        output += table[(b0 & 0x03) << 4];
        output += "==";
    } else if (remaining == 2) {
        unsigned char b0 = data[i], b1 = data[i+1];
        output += table[b0 >> 2];
        output += table[((b0 & 0x03) << 4) | (b1 >> 4)];
        output += table[(b1 & 0x0F) << 2];
        output += "=";
    }

    return output;
}

// compute the Sec-WebSocket-Accept value from the client's key
std::string compute_websocket_accept(const std::string& client_key) {
    std::string combined = client_key + WS_MAGIC;

    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(combined.c_str()),
         combined.length(),
         hash);

    return base64_encode(hash, SHA_DIGEST_LENGTH);
}

// extract any header value from a raw HTTP request
std::string extract_header(const std::string& request, const std::string& header_name) {
    std::string search = header_name + ": ";
    size_t pos = request.find(search);
    if (pos == std::string::npos) return "";

    size_t start = pos + search.length();
    size_t end = request.find("\r\n", start);
    if (end == std::string::npos) return "";

    return request.substr(start, end - start);
}

struct WSFrame {
    bool fin;
    int opcode;
    std::string payload;
};

WSFrame parse_ws_frame(const std::string& data) {
    WSFrame frame;

    unsigned char byte0 = data[0];
    unsigned char byte1 = data[1];

    frame.fin    = (byte0 & 0x80) != 0;
    frame.opcode = byte0 & 0x0F;

    bool masked = (byte1 & 0x80) != 0;
    uint64_t payload_len = byte1 & 0x7F;

    size_t pos = 2;

    if (payload_len == 126) {
        payload_len = ((unsigned char)data[2] << 8) | (unsigned char)data[3];
        pos = 4;
    } else if (payload_len == 127) {
        payload_len = 0;
        for (int i = 0; i < 8; i++) {
            payload_len = (payload_len << 8) | (unsigned char)data[2 + i];
        }
        pos = 10;
    }

    unsigned char masking_key[4] = {0,0,0,0};
    if (masked) {
        for (int i = 0; i < 4; i++) {
            masking_key[i] = data[pos + i];
        }
        pos += 4;
    }

    std::string payload = data.substr(pos, payload_len);

    if (masked) {
        for (size_t i = 0; i < payload.size(); i++) {
            payload[i] = payload[i] ^ masking_key[i % 4];
        }
    }

    frame.payload = payload;
    return frame;
}

std::string build_ws_frame(const std::string& payload, int opcode = 0x1) {
    std::string frame;

    frame += (char)(0x80 | opcode);

    size_t len = payload.size();

    if (len <= 125) {
        frame += (char)len;
    } else if (len <= 65535) {
        frame += (char)126;
        frame += (char)((len >> 8) & 0xFF);
        frame += (char)(len & 0xFF);
    } else {
        frame += (char)127;
        for (int i = 7; i >= 0; i--) {
            frame += (char)((len >> (i * 8)) & 0xFF);
        }
    }

    frame += payload;
    return frame;
}


bool is_websocket_upgrade(const std::string& request) {
    std::string upgrade = extract_header(request, "Upgrade");
    std::transform(upgrade.begin(), upgrade.end(), upgrade.begin(), ::tolower);
    return upgrade == "websocket";
}

void send_websocket_handshake(const std::string& request, int client_fd) {
    std::string client_key = extract_header(request, "Sec-WebSocket-Key");
    std::string accept_key = compute_websocket_accept(client_key);

    std::string response =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + accept_key + "\r\n"
        "\r\n";

    send(client_fd, response.c_str(), response.size(), 0);
}


class ConnectionRegistry {
private:
    std::unordered_map<std::string, int> connections;
    std::mutex mtx;

public:
    void add(const std::string& username, int client_fd) {
        std::lock_guard<std::mutex> lock(mtx);
        connections[username] = client_fd;
    }

    void remove(const std::string& username) {
        std::lock_guard<std::mutex> lock(mtx);
        connections.erase(username);
    }

    bool notify(const std::string& username, const std::string& message) {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = connections.find(username);
        if (it == connections.end()) return false;

        std::string frame = build_ws_frame(message);
        int sent = send(it->second, frame.c_str(), frame.size(), 0);
        return sent > 0;
    }

    bool is_connected(const std::string& username) {
        std::lock_guard<std::mutex> lock(mtx);
        return connections.find(username) != connections.end();
    }
};
