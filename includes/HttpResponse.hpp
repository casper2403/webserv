#ifndef HTTPRESPONSE_HPP
#define HTTPRESPONSE_HPP

#include "HttpRequest.hpp"
#include "Config.hpp"
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <cstdio> // For std::remove

class HttpResponse {
public:
    static std::string generateResponse(const HttpRequest& req, const std::vector<ServerConfig>& configs, int client_port);

private:
    static const ServerConfig* findMatchingServer(const HttpRequest& req, const std::vector<ServerConfig>& configs, int client_port);
    static const LocationConfig* findMatchingLocation(const ServerConfig& server, const std::string& path);

    // Method Handlers
    static std::string handleGetRequest(const LocationConfig& loc_config, const std::string& uri);
    static std::string handleDeleteRequest(const LocationConfig& loc_config, const std::string& uri); // NEW
    static std::string handlePostRequest(const LocationConfig& loc_config, const HttpRequest& req); // NEW (Prototype only for now)

    // Utilities
    static std::string buildResponseHeader(int status_code, const std::string& status_text, size_t content_length, const std::string& content_type);
    static std::string buildErrorResponse(int status_code, const ServerConfig* server_config);
    static std::string getFileContent(const std::string& filepath);
    static std::string getMimeType(const std::string& filepath);
};

#endif