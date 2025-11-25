#ifndef HTTPREQUEST_HPP
#define HTTPREQUEST_HPP

#include <string>
#include <map>
#include <vector>
#include <iostream>
#include <sstream>
#include <cstdlib>

enum RequestState {
    STATE_REQUEST_LINE,
    STATE_HEADERS,
    STATE_BODY,
    STATE_CHUNKED,
    STATE_COMPLETE
};

class HttpRequest {
public:
    HttpRequest();
    ~HttpRequest();

    // Process incoming raw data
    // Returns true if parsing is complete
    bool parse(const std::string& raw_data);

    // Getters
    std::string getMethod() const;
    std::string getPath() const;
    std::string getHeader(const std::string& key) const;
    std::string getBody() const;
    bool isFinished() const;

    // Reset for keep-alive connections
    void reset();

private:
    RequestState _state;
    std::string _method;
    std::string _path;
    std::string _version;
    std::map<std::string, std::string> _headers;
    std::string _body;
    
    // Buffer for accumulated data that hasn't been parsed yet
    std::string _buffer;
    
    // Helpers
    void parseRequestLine();
    void parseHeaders();
    void parseBody();
    void parseChunkedBody(); // We will implement this in the next iteration
    
    // Internal tracking for body size
    size_t _content_length;
};

#endif