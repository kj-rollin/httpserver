#pragma once
#include <string>

// escape special characters for safe JSON output
std::string json_escape(const std::string& input) {
    std::string output;
    for (char c : input) {
        if (c == '"')       output += "\\\"";
        else if (c == '\\') output += "\\\\";
        else if (c == '\n') output += "\\n";
        else if (c == '\r') output += "\\r";
        else if (c == '\t') output += "\\t";
        else                output += c;
    }
    return output;
}

// extract a string field value from simple JSON: {"field":"value"}
std::string extract_json_field(const std::string& json, const std::string& field) {
    std::string search = "\"" + field + "\":\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";

    size_t start = pos + search.size();
    size_t end   = json.find("\"", start);

    return json.substr(start, end - start);
}

// escape HTML special characters to prevent XSS
std::string html_escape(const std::string& input) {
    std::string output;
    for (char c : input) {
        switch (c) {
            case '<':  output += "&lt;";   break;
            case '>':  output += "&gt;";   break;
            case '&':  output += "&amp;";  break;
            case '"':  output += "&quot;"; break;
            case '\'': output += "&#39;";  break;
            default:   output += c;
        }
    }
    return output;
}
