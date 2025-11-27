#ifndef HTTPRESPONSE_HPP
#define HTTPRESPONSE_HPP

#include "HttpRequest.hpp"
#include "Config.hpp"
#include <fstream>
#include <sstream>
#include <sys/stat.h>
class HttpResponse {
public:
    // Main function to process the request and generate a response string
    static std::string generateResponse(const HttpRequest& req, const std::vector<ServerConfig>& configs, int client_port);

private:
    // Helper 1: Configuration matching
    static const ServerConfig* findMatchingServer(const HttpRequest& req, const std::vector<ServerConfig>& configs, int client_port);
    static const LocationConfig* findMatchingLocation(const ServerConfig& server, const std::string& path);

    // Helper 2: Static file serving
    static std::string handleGetRequest(const LocationConfig& loc_config, const std::string& uri);

    // Helper 3: Response building
    static std::string buildResponseHeader(int status_code, const std::string& status_text, size_t content_length, const std::string& content_type);
    static std::string buildErrorResponse(int status_code, const ServerConfig* server_config);
    static std::string getFileContent(const std::string& filepath);
    static std::string getMimeType(const std::string& filepath);
};

#endif