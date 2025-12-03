#include "../includes/HttpResponse.hpp"
#include <ctime> // Added for timestamp generation

// --- 2.2 Location Matching (Simplified for now) ---
/**
 * @brief Find the best matching location block for a given request path
 * 
 * @param server The server configuration
 * @param path The request URI path
 * @return const LocationConfig* Pointer to the matching location config, or nullptr if not found
 */
const LocationConfig* HttpResponse::findMatchingLocation(const ServerConfig& server, const std::string& path) {
    const LocationConfig* best_match = NULL;
    size_t best_match_length = 0;
    
    for (size_t i = 0; i < server.locations.size(); ++i) {
        const std::string& location_path = server.locations[i].path;
        
        if (path.find(location_path) == 0) {
            if (location_path.length() > best_match_length) {
                best_match = &server.locations[i];
                best_match_length = location_path.length();
            }
        }
    }
    return best_match;
}

// --- 3.1 & 3.2 Path Resolution and File Read ---
/**
 * @brief Handle GET requests by serving files
 * 
 * @param loc_config The location configuration
 * @param uri The request URI
 * @return std::string The full HTTP response
 */
std::string HttpResponse::handleGetRequest(const LocationConfig& loc_config, const std::string& uri) {
    std::string filepath;

    if (uri == "/") {
        filepath = loc_config.root + "/" + loc_config.index;
    } else {
        filepath = loc_config.root + uri;
    }

    struct stat file_stat;
    if (stat(filepath.c_str(), &file_stat) != 0) {
        return buildErrorResponse(404, NULL);
    }

    if (S_ISDIR(file_stat.st_mode)) {
        return buildErrorResponse(403, NULL);
    }

    std::string content = getFileContent(filepath);
    std::string mime_type = getMimeType(filepath);
    
    std::string header = buildResponseHeader(200, "OK", content.length(), mime_type);
    return header + content;
}

// Simplified File Content Reader
/**
 * @brief Read the entire content of a file into a string
 * 
 * @param filepath The path to the file
 * @return std::string The file content
 */
std::string HttpResponse::getFileContent(const std::string& filepath) {
    std::ifstream ifs(filepath.c_str());
    std::stringstream buffer;
    buffer << ifs.rdbuf();
    return buffer.str();
}

/**
 * @brief Get the MIME type based on the file extension
 * 
 * @param filepath The path to the file
 * @return std::string The MIME type
 */
std::string HttpResponse::getMimeType(const std::string& filepath) {
    if (filepath.rfind(".html") != std::string::npos) return "text/html";
    if (filepath.rfind(".css") != std::string::npos) return "text/css";
    if (filepath.rfind(".jpg") != std::string::npos || filepath.rfind(".jpeg") != std::string::npos) return "image/jpeg";
    if (filepath.rfind(".png") != std::string::npos) return "image/png";
    return "text/plain";
}

/**
 * @brief Build the HTTP response header
 * 
 * @param status_code The HTTP status code
 * @param status_text The HTTP status text
 * @param content_length The length of the response body
 * @param content_type The MIME type of the response body
 * @return std::string The complete HTTP response header
 */
std::string HttpResponse::buildResponseHeader(int status_code, const std::string& status_text, size_t content_length, const std::string& content_type) {
    std::stringstream ss;
    ss << "HTTP/1.1 " << status_code << " " << status_text << "\r\n";
    ss << "Content-Type: " << content_type << "\r\n";
    ss << "Content-Length: " << content_length << "\r\n";
    ss << "Connection: keep-alive\r\n";
    ss << "\r\n";
    return ss.str();
}

// --- 4.1 Simple Error Response Generator ---
/**
 * @brief Build a simple error response page
 * 
 * @param status_code The HTTP status code
 * @param server_config The server configuration (for custom error pages, if any)
 * @return std::string The full HTTP error response
 */
std::string HttpResponse::buildErrorResponse(int status_code, const ServerConfig* server_config) {
    (void)server_config;
    
    std::string status_text;
    std::string body;

    if (status_code == 404) {
        status_text = "Not Found";
        body = "<h1>404 Not Found</h1><p>The requested resource was not found.</p>";
    } else if (status_code == 405) {
        status_text = "Method Not Allowed";
        body = "<h1>405 Method Not Allowed</h1>";
    } else if (status_code == 403) {
        status_text = "Forbidden";
        body = "<h1>403 Forbidden</h1>";
    } else {
        status_text = "Internal Server Error";
        body = "<h1>500 Internal Server Error</h1>";
    }

    std::string header = buildResponseHeader(status_code, status_text, body.length(), "text/html");
    return header + body;
}

/**
 * @brief Find the best matching server block for a given request
 * 
 * @param req The HTTP request
 * @param configs The list of server configurations
 * @param client_port The port of the client
 * @return const ServerConfig* Pointer to the matching server config, or nullptr if not found
 */
const ServerConfig* HttpResponse::findMatchingServer(const HttpRequest& req, const std::vector<ServerConfig>& configs, int client_port) {
    (void)req;
    
    for (size_t i = 0; i < configs.size(); ++i) {
        if (configs[i].port == client_port) {
            return &configs[i];
        }
    }
    if (!configs.empty()) {
        return &configs[0];
    }
    return NULL;
}

/**
 * @brief Generate the HTTP response for a given request
 * 
 * @param req The HTTP request
 * @param configs The list of server configurations
 * @param client_port The port of the client
 * @return std::string The full HTTP response
 */
std::string HttpResponse::generateResponse(const HttpRequest& req, const std::vector<ServerConfig>& configs, int client_port) {
    const ServerConfig* server_config = findMatchingServer(req, configs, client_port);
    if (!server_config) return buildErrorResponse(500, NULL);
    
    const LocationConfig* loc_config = findMatchingLocation(*server_config, req.getPath());
    if (!loc_config) return buildErrorResponse(404, server_config); 

    bool method_allowed = false;
    if (loc_config->methods.empty()) {
        if (req.getMethod() == "GET") method_allowed = true;
    } else {
        for (size_t i = 0; i < loc_config->methods.size(); ++i) {
            if (loc_config->methods[i] == req.getMethod()) {
                method_allowed = true;
                break;
            }
        }
    }

    if (!method_allowed) return buildErrorResponse(405, server_config);

    if (req.getMethod() == "GET") {
        return handleGetRequest(*loc_config, req.getPath());
    } else if (req.getMethod() == "DELETE") {
        return handleDeleteRequest(*loc_config, req.getPath());
    } else if (req.getMethod() == "POST") {
        return handlePostRequest(*loc_config, req);
    } else {
        return buildErrorResponse(501, server_config);
    }
}

// --- DELETE HANDLER ---
/**
 * @brief Handle DELETE requests by removing files
 * 
 * @param loc_config The location configuration
 * @param uri The request URI
 * @return std::string The full HTTP response
 */
std::string HttpResponse::handleDeleteRequest(const LocationConfig& loc_config, const std::string& uri) {
    std::string filepath = loc_config.root + uri;

    struct stat file_stat;
    if (stat(filepath.c_str(), &file_stat) != 0) {
        return buildErrorResponse(404, NULL);
    }

    if (S_ISDIR(file_stat.st_mode)) {
        return buildErrorResponse(403, NULL); 
    }

    if (std::remove(filepath.c_str()) != 0) {
        return buildErrorResponse(500, NULL);
    }

    return buildResponseHeader(204, "No Content", 0, "");
}

// --- POST HANDLER (File Upload) ---
/**
 * @brief Handle POST requests by saving uploaded files
 * 
 * @param loc_config The location configuration
 * @param req The HTTP request
 * @return std::string The full HTTP response
 */
std::string HttpResponse::handlePostRequest(const LocationConfig& loc_config, const HttpRequest& req) {
    // 1. Determine the final file path
    std::string full_path = loc_config.root + req.getPath();
    
    // 2. Check if the path is a directory
    // If it is, we generate a unique filename: upload_<timestamp>.dat
    struct stat st;
    if (stat(full_path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
        // Ensure path ends with slash
        if (full_path[full_path.length() - 1] != '/') {
            full_path += "/";
        }
        
        std::stringstream ss;
        ss << "upload_" << std::time(NULL) << ".dat";
        full_path += ss.str();
    }

    // 3. Write the file
    // We use binary mode to correctly handle images/pdfs/etc.
    std::ofstream outfile(full_path.c_str(), std::ios::binary);
    
    if (!outfile.is_open()) {
        std::cerr << "Error: Could not write to " << full_path << std::endl;
        return buildErrorResponse(500, NULL);
    }

    outfile << req.getBody();
    outfile.close();

    // 4. Return 201 Created
    std::string body = "File created successfully: " + full_path;
    return buildResponseHeader(201, "Created", body.length(), "text/plain") + body;
}