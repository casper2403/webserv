#include "../includes/HttpRequest.hpp"

/**
 * @brief Construct a new HttpRequest object
 * 
 */
HttpRequest::HttpRequest() 
    : _state(STATE_REQUEST_LINE), _content_length(0), _chunk_length(0), _is_chunk_size(true) {}

HttpRequest::~HttpRequest() {}

/**
 * @brief Reset the HttpRequest object to initial state for keep-alive connections
 * 
 */
void HttpRequest::reset() {
    _state = STATE_REQUEST_LINE;
    _method.clear();
    _path.clear();
    _version.clear();
    _headers.clear();
    _body.clear();
    _buffer.clear();
    _content_length = 0;
    _chunk_length = 0;
    _is_chunk_size = true;
}

std::string HttpRequest::getMethod() const { return _method; }
std::string HttpRequest::getPath() const { return _path; }
std::string HttpRequest::getBody() const { return _body; }
bool HttpRequest::isFinished() const { return _state == STATE_COMPLETE; }

/**
 * @brief Get the value of a specific header
 * 
 * @param key Header name
 * @return std::string Header value or empty string if not found
 */
std::string HttpRequest::getHeader(const std::string& key) const {
    std::map<std::string, std::string>::const_iterator it = _headers.find(key);
    if (it != _headers.end()) return it->second;
    return "";
}

/**
 * @brief Parse incoming raw data
 * 
 * @param raw_data Incoming data chunk
 * @return true if parsing is complete
 */
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
    if (_state == STATE_CHUNKED) {
        parseChunkedBody();
    }
    
    return _state == STATE_COMPLETE;
}

/**
 * @brief Parse the request line (e.g., "GET /path HTTP/1.1")
 * 
 */
void HttpRequest::parseRequestLine() {
    size_t pos = _buffer.find("\r\n");
    if (pos == std::string::npos) return;

    std::string line = _buffer.substr(0, pos);
    _buffer.erase(0, pos + 2);

    std::stringstream ss(line);
    ss >> _method >> _path >> _version;

    if (_method.empty() || _path.empty() || _version.empty()) {
        std::cerr << "Error: Malformed request line" << std::endl;
        _state = STATE_COMPLETE;
        return;
    }
    _state = STATE_HEADERS;
}

/**
 * @brief Parse HTTP headers from the buffer
 * 
 */
void HttpRequest::parseHeaders() {
    size_t pos;
    while ((pos = _buffer.find("\r\n")) != std::string::npos) {
        if (pos == 0) {
            _buffer.erase(0, 2); // End of headers
            
            // Determine next state
            if (_headers.count("Content-Length")) {
                _content_length = std::atoi(_headers["Content-Length"].c_str());
                if (_content_length > 0) {
                    _state = STATE_BODY;
                } else {
                    _state = STATE_COMPLETE;
                }
            } 
            else if (_headers.count("Transfer-Encoding") && _headers["Transfer-Encoding"] == "chunked") {
                _state = STATE_CHUNKED;
            } 
            else {
                _state = STATE_COMPLETE;
            }
            return;
        }

        std::string line = _buffer.substr(0, pos);
        _buffer.erase(0, pos + 2);

        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            std::string value = line.substr(colon + 1);
            while (!value.empty() && value[0] == ' ') value.erase(0, 1);
            _headers[key] = value;
        }
    }
}

/**
 * @brief Parse the body based on Content-Length
 * 
 */
void HttpRequest::parseBody() {
    if (_buffer.size() >= _content_length) {
        _body = _buffer.substr(0, _content_length);
        _buffer.erase(0, _content_length);
        _state = STATE_COMPLETE;
    }
}

/**
 * @brief Parse chunked transfer encoded body
 * 
 */
void HttpRequest::parseChunkedBody() {
    while (true) {
        if (_is_chunk_size) {
            // 1. Expecting <Hex Size>\r\n
            size_t pos = _buffer.find("\r\n");
            if (pos == std::string::npos) return; // Wait for more data

            std::string line = _buffer.substr(0, pos);
            _buffer.erase(0, pos + 2);

            // Parse Hex
            std::stringstream ss;
            ss << std::hex << line;
            ss >> _chunk_length;

            if (_chunk_length == 0) {
                // End of chunks. 
                // Technically there might be trailers, but we'll assume end of request.
                // Depending on implementation, we might need to consume one last \r\n
                _state = STATE_COMPLETE;
                return;
            }
            _is_chunk_size = false; // Next step: Read data
        } 
        else {
            // 2. Expecting <Data>\r\n
            // We need to wait until we have the full chunk + CRLF
            if (_buffer.size() < _chunk_length + 2) return; 

            // Extract data
            _body += _buffer.substr(0, _chunk_length);
            _buffer.erase(0, _chunk_length + 2); // Remove data + CRLF

            // Reset to read next chunk size
            _is_chunk_size = true;
        }
    }
}