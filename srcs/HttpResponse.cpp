#include "../includes/HttpResponse.hpp"

// --- 2.2 Location Matching (Simplified for now) ---
const LocationConfig* HttpResponse::findMatchingLocation(const ServerConfig& server, const std::string& path) {
    // Find the longest, most specific match by checking if path starts with location path
    const LocationConfig* best_match = NULL;
    size_t best_match_length = 0;
    
    for (size_t i = 0; i < server.locations.size(); ++i) {
        const std::string& location_path = server.locations[i].path;
        
        // Check if the requested path starts with this location path
        if (path.find(location_path) == 0) {
            // This location matches, check if it's more specific than current best
            if (location_path.length() > best_match_length) {
                best_match = &server.locations[i];
                best_match_length = location_path.length();
            }
        }
    }
    
    return best_match;
}

// --- 3.1 & 3.2 Path Resolution and File Read ---
std::string HttpResponse::handleGetRequest(const LocationConfig& loc_config, const std::string& uri) {
    std::string filepath;

    if (uri == "/") {
        // Concatenate root + index for homepage
        filepath = loc_config.root + "/" + loc_config.index;
    } else {
        // For other files, concatenate root + uri
        filepath = loc_config.root + uri;
    }

    // Check if file exists and is accessible
    struct stat file_stat;
    if (stat(filepath.c_str(), &file_stat) != 0) {
        // File not found (or no permission)
        return buildErrorResponse(404, NULL); // Use NULL for config for now
    }

    if (S_ISDIR(file_stat.st_mode)) {
        // It's a directory, should look for index or autoindex
        return buildErrorResponse(403, NULL); // Forbidden for simplicity for now
    }

    // --- 3.3 Response Generation ---
    std::string content = getFileContent(filepath);
    std::string mime_type = getMimeType(filepath);
    
    // Combine header and body
    std::string header = buildResponseHeader(200, "OK", content.length(), mime_type);
    return header + content;
}

// Simplified File Content Reader
std::string HttpResponse::getFileContent(const std::string& filepath) {
    std::ifstream ifs(filepath.c_str());
    std::stringstream buffer;
    buffer << ifs.rdbuf();
    return buffer.str();
}

// Simplistic Mime Type Detector (Needs to be expanded)
std::string HttpResponse::getMimeType(const std::string& filepath) {
    if (filepath.rfind(".html") != std::string::npos) return "text/html";
    if (filepath.rfind(".css") != std::string::npos) return "text/css";
    if (filepath.rfind(".jpg") != std::string::npos || filepath.rfind(".jpeg") != std::string::npos) return "image/jpeg";
    if (filepath.rfind(".png") != std::string::npos) return "image/png";
    return "text/plain"; // Default
}

// Response Header Builder ---
std::string HttpResponse::buildResponseHeader(int status_code, const std::string& status_text, size_t content_length, const std::string& content_type) {
    std::stringstream ss;
    ss << "HTTP/1.1 " << status_code << " " << status_text << "\r\n";
    ss << "Content-Type: " << content_type << "\r\n";
    ss << "Content-Length: " << content_length << "\r\n";
    ss << "Connection: keep-alive\r\n";
    ss << "\r\n"; // End of headers
    return ss.str();
}

// --- 4.1 Simple Error Response Generator ---
std::string HttpResponse::buildErrorResponse(int status_code, const ServerConfig* server_config) {
    (void)server_config; // ADD THIS LINE to suppress unused warning
    
    std::string status_text;
    std::string body;

    // A simple switch for mandatory errors
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

    // TODO: In a later step, check server_config->error_pages map for custom pages

    std::string header = buildResponseHeader(status_code, status_text, body.length(), "text/html");
    return header + body;
}

const ServerConfig* HttpResponse::findMatchingServer(const HttpRequest& req, const std::vector<ServerConfig>& configs, int client_port) {
    (void)req; // ADD THIS LINE to suppress unused warning
    
    // First try to match by port
    for (size_t i = 0; i < configs.size(); ++i) {
        if (configs[i].port == client_port) {
            // TODO: Later add Host header matching for virtual hosts
            return &configs[i];
        }
    }
    // Return first server as fallback
    if (!configs.empty()) {
        return &configs[0];
    }
    return NULL;
}

std::string HttpResponse::generateResponse(const HttpRequest& req, const std::vector<ServerConfig>& configs, int client_port) {
    const ServerConfig* server_config = findMatchingServer(req, configs, client_port);
    if (!server_config) return buildErrorResponse(500, NULL);
    
    const LocationConfig* loc_config = findMatchingLocation(*server_config, req.getPath());
    if (!loc_config) return buildErrorResponse(404, server_config); 

    // Method Allowed Check
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

    // Dispatch
    if (req.getMethod() == "GET") {
        return handleGetRequest(*loc_config, req.getPath());
    } else if (req.getMethod() == "DELETE") {
        return handleDeleteRequest(*loc_config, req.getPath());
    } else if (req.getMethod() == "POST") {
        return handlePostRequest(*loc_config, req);
    } else {
        return buildErrorResponse(501, server_config); // Not Implemented
    }
}

// ... [Keep handleGetRequest] ...

// --- DELETE HANDLER (New) ---
std::string HttpResponse::handleDeleteRequest(const LocationConfig& loc_config, const std::string& uri) {
    // 1. Construct the filepath
    // NOTE: This logic assumes the uri contains the full path relative to root.
    // Real NGINX logic for 'alias' vs 'root' is complex, but for this project:
    std::string filepath = loc_config.root + uri;

    // 2. Check if file exists
    struct stat file_stat;
    if (stat(filepath.c_str(), &file_stat) != 0) {
        // File not found
        return buildErrorResponse(404, NULL);
    }

    // 3. Check if it is a directory
    if (S_ISDIR(file_stat.st_mode)) {
        // We do not allow deleting directories in this simple server
        return buildErrorResponse(403, NULL); 
    }

    // 4. Attempt to delete
    if (std::remove(filepath.c_str()) != 0) {
        // Deletion failed (permissions, locking, etc.)
        return buildErrorResponse(500, NULL);
    }

    // 5. Success (204 No Content is standard for DELETE)
    return buildResponseHeader(204, "No Content", 0, "");
}

// --- POST HANDLER (Stub) ---
std::string HttpResponse::handlePostRequest(const LocationConfig& loc_config, const HttpRequest& req) {
    (void)loc_config;
    (void)req;
    // We will implement Uploads here in the next step
    return buildResponseHeader(200, "OK", 17, "text/plain") + "POST Not Ready Yet";
}