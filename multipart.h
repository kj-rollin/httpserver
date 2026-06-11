#pragma once
#include <string>
#include <vector>

struct UploadedFile {
    std::string filename;
    std::string content_type;
    std::string data;  // raw bytes
};

// declare first — used before definition below
UploadedFile parse_part(const std::string& part);

std::vector<UploadedFile> parse_multipart(const std::string& body,
                                          const std::string& boundary) {
    std::vector<UploadedFile> files;
    std::string delimiter = "--" + boundary;

    size_t pos = 0;
    while (true) {
        size_t start = body.find(delimiter, pos);
        if (start == std::string::npos) break;

        size_t part_start = start + delimiter.size();
        size_t next = body.find(delimiter, part_start);
        if (next == std::string::npos) break;

        std::string part = body.substr(part_start, next - part_start);

        UploadedFile file = parse_part(part);
        if (!file.filename.empty()) {
            files.push_back(file);
        }

        pos = next;
    }

    return files;
}

UploadedFile parse_part(const std::string& part) {
    UploadedFile file;

    size_t header_end = part.find("\r\n\r\n");
    if (header_end == std::string::npos) return file;

    std::string headers = part.substr(0, header_end);
    std::string data    = part.substr(header_end + 4);

    size_t fn_pos = headers.find("filename=\"");
    if (fn_pos != std::string::npos) {
        size_t fn_start = fn_pos + 10;
        size_t fn_end   = headers.find("\"", fn_start);
        file.filename = headers.substr(fn_start, fn_end - fn_start);
    }

    size_t ct_pos = headers.find("Content-Type: ");
    if (ct_pos != std::string::npos) {
        size_t ct_start = ct_pos + 14;
        size_t ct_end   = headers.find("\r\n", ct_start);
        file.content_type = headers.substr(ct_start, ct_end - ct_start);
    }

    if (data.size() >= 2 && data.substr(data.size()-2) == "\r\n")
        data = data.substr(0, data.size()-2);

    file.data = data;
    return file;
}