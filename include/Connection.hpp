#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include <string>
#include <sys/types.h>

class Connection {
public:
    Connection();
    explicit Connection(int fd);

    int fd() const;
    bool isClosed() const;
    bool readyToWrite() const;
    bool hasResponse() const;
    struct pollfd toPollfd(short events) const;

    void appendRequest(const char *data, ssize_t bytes);
    bool parseRequest();
    HttpRequest &request();
    HttpResponse &response();

    void setClosed();

private:
    int _fd;
    bool _closed;
    std::string _buffer;
    HttpRequest _request;
    HttpResponse _response;
};

#endif
