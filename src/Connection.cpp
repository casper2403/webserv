#include "Connection.hpp"
#include <poll.h>

Connection::Connection() : _fd(-1), _closed(true) {}

Connection::Connection(int fd) : _fd(fd), _closed(false) {}

int Connection::fd() const {
    return _fd;
}

bool Connection::isClosed() const {
    return _closed;
}

bool Connection::readyToWrite() const {
    return !_response.raw().empty();
}

bool Connection::hasResponse() const {
    return !_response.raw().empty();
}

struct pollfd Connection::toPollfd(short events) const {
    struct pollfd p;
    p.fd = _fd;
    p.events = events;
    p.revents = 0;
    return p;
}

void Connection::appendRequest(const char *data, ssize_t bytes) {
    _buffer.append(data, bytes);
}

bool Connection::parseRequest() {
    size_t end = _buffer.find("\r\n\r\n");
    if (end == std::string::npos) {
        return false;
    }
    std::string raw = _buffer.substr(0, end + 4);
    _buffer.erase(0, end + 4);
    return _request.parse(raw);
}

HttpRequest &Connection::request() {
    return _request;
}

HttpResponse &Connection::response() {
    return _response;
}

void Connection::setClosed() {
    _closed = true;
}
