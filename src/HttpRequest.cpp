#include "HttpRequest.hpp"
#include "Utils.hpp"
#include <sstream>

HttpRequest::HttpRequest() {
    clear();
}

void HttpRequest::clear() {
    _method.clear();
    _path.clear();
    _headers.clear();
}

bool HttpRequest::parse(const std::string &raw) {
    clear();
    std::istringstream stream(raw);
    std::string line;
    if (!std::getline(stream, line)) {
        return false;
    }
    if (!line.empty() && line[line.size() - 1] == '\r') {
        line.erase(line.size() - 1);
    }
    std::istringstream requestLine(line);
    requestLine >> _method >> _path;
    if (_method.empty() || _path.empty()) {
        return false;
    }
    while (std::getline(stream, line)) {
        if (line == "\r" || line == "") {
            break;
        }
        if (!line.empty() && line[line.size() - 1] == '\r') {
            line.erase(line.size() - 1);
        }
        size_t colon = line.find(":");
        if (colon != std::string::npos) {
            std::string key = trim(line.substr(0, colon));
            std::string value = trim(line.substr(colon + 1));
            _headers[key] = value;
        }
    }
    return true;
}

const std::string &HttpRequest::method() const {
    return _method;
}

const std::string &HttpRequest::path() const {
    return _path;
}

const std::map<std::string, std::string> &HttpRequest::headers() const {
    return _headers;
}
