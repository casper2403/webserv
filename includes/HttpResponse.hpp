#ifndef HTTPRESPONSE_HPP
#define HTTPRESPONSE_HPP

#include "HttpRequest.hpp"
#include "Config.hpp"
#include "Webserver.hpp" // For Client struct
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <cstdio>

class HttpResponse {
public:
    // Main entry point - modifies Client state directly
    static void processRequest(Client& client, const std::vector<ServerConfig>& configs);
    
    // Helper to finish CGI processing
    static std::string buildCgiResponse(const std::string& cgi_output);

private:
    static const ServerConfig* findMatchingServer(const HttpRequest& req, const std::vector<ServerConfig>& configs, int client_port);
    static const LocationConfig* findMatchingLocation(const ServerConfig& server, const std::string& path);

    static std::string handleGetRequest(const LocationConfig& loc_config, const std::string& uri);
    static std::string handleDeleteRequest(const LocationConfig& loc_config, const std::string& uri);
    static std::string handlePostRequest(const LocationConfig& loc_config, const HttpRequest& req);
    
    // CGI now sets state in Client instead of returning string
    static void handleCgiRequest(Client& client, const LocationConfig& loc_config, const std::string& script_path);
    static bool isCgiRequest(const LocationConfig& loc_config, const std::string& path);

    static std::string buildResponseHeader(int status_code, const std::string& status_text, size_t content_length, const std::string& content_type);
    static std::string buildRedirectResponse(int status_code, const std::string& location);
    static std::string buildErrorResponse(int status_code, const ServerConfig* server_config);
    
    static std::string getFileContent(const std::string& filepath);
    static std::string getMimeType(const std::string& filepath);
    static std::string generateDirectoryListing(const std::string& directory_path, const std::string& request_uri);
};

#endif