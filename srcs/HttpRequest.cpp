#include "../includes/HttpRequest.hpp"

HttpRequest::HttpRequest() : _state(STATE_REQUEST_LINE), _content_length(0) {}
HttpRequest::~HttpRequest() {}

void HttpRequest::reset() {
    _state = STATE_REQUEST_LINE;
    _method.clear();
    _path.clear();
    _version.clear();
    _headers.clear();
    _body.clear();
    _buffer.clear();
    _content_length = 0;
}

std::string HttpRequest::getMethod() const { return _method; }
std::string HttpRequest::getPath() const { return _path; }
std::string HttpRequest::getBody() const { return _body; }
bool HttpRequest::isFinished() const { return _state == STATE_COMPLETE; }

std::string HttpRequest::getHeader(const std::string& key) const {
    std::map<std::string, std::string>::const_iterator it = _headers.find(key);
    if (it != _headers.end()) return it->second;
    return "";
}

bool HttpRequest::parse(const std::string& raw_data) {
    _buffer += raw_data;

    if (_state == STATE_REQUEST_LINE) {
        parseRequestLine();
    }
    if (_state == STATE_HEADERS) {
        parseHeaders();
    }
    if (_state == STATE_BODY) {
        parseBody();
    }
    // We will add STATE_CHUNKED later
    
    return _state == STATE_COMPLETE;
}

void HttpRequest::parseRequestLine() {
    size_t pos = _buffer.find("\r\n");
    if (pos == std::string::npos) return; // Not enough data yet

    std::string line = _buffer.substr(0, pos);
    _buffer.erase(0, pos + 2); // Remove line + CRLF

    std::stringstream ss(line);
    ss >> _method >> _path >> _version;

    if (_method.empty() || _path.empty() || _version.empty()) {
        // Handle error: Bad Request (for now just print)
        std::cerr << "Error: Malformed request line" << std::endl;
        _state = STATE_COMPLETE; // Force stop
        return;
    }
    _state = STATE_HEADERS;
}

void HttpRequest::parseHeaders() {
    size_t pos;
    while ((pos = _buffer.find("\r\n")) != std::string::npos) {
        // Empty line means end of headers
        if (pos == 0) {
            _buffer.erase(0, 2);
            
            // DECIDE NEXT STATE: Body or Complete?
            if (_headers.count("Content-Length")) {
                _content_length = std::atoi(_headers["Content-Length"].c_str());
                if (_content_length > 0) {
                    _state = STATE_BODY;
                } else {
                    _state = STATE_COMPLETE;
                }
            } 
            else if (_headers.count("Transfer-Encoding") && _headers["Transfer-Encoding"] == "chunked") {
                _state = STATE_CHUNKED; // To be implemented
                // For now, mark complete to avoid hanging
                 std::cerr << "Chunked not implemented yet" << std::endl;
                _state = STATE_COMPLETE; 
            } 
            else {
                _state = STATE_COMPLETE; // No body
            }
            return;
        }

        std::string line = _buffer.substr(0, pos);
        _buffer.erase(0, pos + 2);

        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            std::string value = line.substr(colon + 1);
            
            // Trim whitespace
            while (!value.empty() && value[0] == ' ') value.erase(0, 1);
            
            _headers[key] = value;
        }
    }
}

void HttpRequest::parseBody() {
    if (_buffer.size() >= _content_length) {
        _body = _buffer.substr(0, _content_length);
        _buffer.erase(0, _content_length);
        _state = STATE_COMPLETE;
    }
}