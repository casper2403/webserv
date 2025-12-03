#include "../includes/HttpRequest.hpp"

/**
 * @class HttpRequest
 * @brief Represents and parses an HTTP request, supporting both standard and chunked transfer encoding.
 */

/**
 * @brief Construct a new HttpRequest object and initialize its state.
 */
HttpRequest::HttpRequest() 
    : _state(STATE_REQUEST_LINE), _content_length(0), _chunk_length(0), _is_chunk_size(true) {}

/**
 * @brief Destroy the HttpRequest object.
 */
HttpRequest::~HttpRequest() {}

/**
 * @brief Reset the HttpRequest object to its initial state for reuse (e.g., for keep-alive connections).
 * 
 * This function clears all parsed data, including method, path, version, headers, body, and buffer.
 * It also resets the content length, chunk length, and chunk size indicator.
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

/**
 * @brief Get the HTTP method (e.g., GET, POST).
 * @return The HTTP method as a string.
 */
std::string HttpRequest::getMethod() const { return _method; }

/**
 * @brief Get the requested path from the HTTP request line.
 * @return The request path as a string.
 */
std::string HttpRequest::getPath() const { return _path; }

/**
 * @brief Get the body of the HTTP request.
 * @return The request body as a string.
 */
std::string HttpRequest::getBody() const { return _body; }

/**
 * @brief Check if the HTTP request has been fully parsed.
 * @return True if parsing is complete, false otherwise.
 */
bool HttpRequest::isFinished() const { return _state == STATE_COMPLETE; }

/**
 * @brief Get the value of a specific HTTP header.
 * @param key The header name.
 * @return The header value, or an empty string if not found.
 */
std::string HttpRequest::getHeader(const std::string& key) const {
    std::map<std::string, std::string>::const_iterator it = _headers.find(key);
    if (it != _headers.end()) return it->second;
    return "";
}

/**
 * @brief Parse incoming raw data and update the request state.
 *
 * Appends new data to the buffer and processes it according to the current state.
 * @param raw_data The incoming data chunk.
 * @return True if the request is fully parsed, false otherwise.
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
 * @brief Parse the HTTP request line (e.g., "GET /path HTTP/1.1").
 *
 * Extracts the method, path, and version from the first line of the request.
 * Sets the state to STATE_HEADERS if successful, or STATE_COMPLETE on error.
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
 * @brief Parse HTTP headers from the buffer.
 *
 * Reads headers line by line until an empty line is found, then determines the next state
 * based on Content-Length or Transfer-Encoding headers.
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
 * @brief Parse the request body based on Content-Length.
 *
 * Reads the specified number of bytes from the buffer into the body.
 * Sets the state to STATE_COMPLETE when done.
 */
void HttpRequest::parseBody() {
    if (_buffer.size() >= _content_length) {
        _body = _buffer.substr(0, _content_length);
        _buffer.erase(0, _content_length);
        _state = STATE_COMPLETE;
    }
}

/**
 * @brief Parse a chunked transfer-encoded HTTP request body.
 *
 * Handles chunk size parsing, data extraction, and state transitions for chunked encoding.
 * Sets the state to STATE_COMPLETE when the last chunk is received.
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
                // End of chunks. Consume trailing CRLF
                if (_buffer.substr(0, 2) == "\r\n" && _buffer.size() >= 2) {
                    _buffer.erase(0, 2);
                }
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