#include "HttpResponse.hpp"
#include <sstream>

HttpResponse::HttpResponse() : _statusCode(200), _statusMessage("OK") {}

void HttpResponse::reset() {
    _statusCode = 200;
    _statusMessage = "OK";
    _headers.clear();
    _body.clear();
    _raw.clear();
}

void HttpResponse::setStatus(int code, const std::string &message) {
    _statusCode = code;
    _statusMessage = message;
    build();
}

void HttpResponse::setHeader(const std::string &key, const std::string &value) {
    _headers[key] = value;
    build();
}

void HttpResponse::setBody(const std::string &body) {
    _body = body;
    build();
}

const std::string &HttpResponse::raw() const {
    return _raw;
}

void HttpResponse::build() {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << _statusCode << " " << _statusMessage << "\r\n";
    std::map<std::string, std::string>::const_iterator it = _headers.begin();
    for (; it != _headers.end(); ++it) {
        oss << it->first << ": " << it->second << "\r\n";
    }
    if (!_body.empty()) {
        oss << "Content-Length: " << _body.size() << "\r\n";
    }
    oss << "\r\n";
    oss << _body;
    _raw = oss.str();
}
