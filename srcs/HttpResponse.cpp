#include "../includes/HttpResponse.hpp"
#include <ctime> // Added for timestamp generation
#include <dirent.h> // Added for directory listing

// --- 2.2 Location Matching (Simplified for now) ---
/**
 * @brief Finds the best matching location block for a given request path.
 *
 * This function iterates through the server's location blocks and returns the one
 * with the longest matching prefix for the given path. If no match is found, returns nullptr.
 *
 * @param server The server configuration containing location blocks.
 * @param path The request URI path to match.
 * @return Pointer to the best matching LocationConfig, or nullptr if none found.
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
 * @brief Handles GET requests by serving files from the filesystem.
 *
 * Resolves the requested URI to a file path using the location configuration.
 * Handles directory requests by checking for index files or generating autoindex listings.
 * Returns appropriate error responses for missing files or forbidden access.
 *
 * @param loc_config The location configuration for the request.
 * @param uri The request URI.
 * @return The full HTTP response as a string.
 */
std::string HttpResponse::handleGetRequest(const LocationConfig& loc_config, const std::string& uri) {
    // Resolve the file path
    std::string filepath;
    if (uri == "/") {
        filepath = loc_config.root;
    } else {
        filepath = loc_config.root + uri;
    }

    // Check if path exists
    struct stat file_stat;
    if (stat(filepath.c_str(), &file_stat) != 0) {
        return buildErrorResponse(404, NULL);
    }

    // If it's a regular file, serve it directly
    if (S_ISREG(file_stat.st_mode)) {
        std::string content = getFileContent(filepath);
        std::string mime_type = getMimeType(filepath);
        std::string header = buildResponseHeader(200, "OK", content.length(), mime_type);
        return header + content;
    }

    // If it's a directory, handle index file and autoindex
    if (S_ISDIR(file_stat.st_mode)) {
        // Ensure the directory path ends with a slash for consistency
        std::string dir_path = filepath;
        if (dir_path[dir_path.length() - 1] != '/') {
            dir_path += "/";
        }

        // Try to serve the index file if specified
        if (!loc_config.index.empty()) {
            std::string index_path = dir_path + loc_config.index;
            struct stat index_stat;
            if (stat(index_path.c_str(), &index_stat) == 0 && S_ISREG(index_stat.st_mode)) {
                std::string content = getFileContent(index_path);
                std::string mime_type = getMimeType(index_path);
                std::string header = buildResponseHeader(200, "OK", content.length(), mime_type);
                return header + content;
            }
        }

        // If no index file found, check autoindex
        if (loc_config.autoindex) {
            std::string listing = generateDirectoryListing(dir_path, uri);
            if (!listing.empty()) {
                std::string header = buildResponseHeader(200, "OK", listing.length(), "text/html");
                return header + listing;
            }
        }

        // If neither index file nor autoindex, return 403 Forbidden
        return buildErrorResponse(403, NULL);
    }

    // For any other file type (e.g., special files), return 403 Forbidden
    return buildErrorResponse(403, NULL);
}

// Simplified File Content Reader
/**
 * @brief Reads the entire content of a file into a string.
 *
 * Opens the file at the given path and reads its contents. If the file cannot be opened,
 * the returned string will be empty.
 *
 * @param filepath The path to the file.
 * @return The file content as a string.
 */
std::string HttpResponse::getFileContent(const std::string& filepath) {
    std::ifstream ifs(filepath.c_str());
    std::stringstream buffer;
    buffer << ifs.rdbuf();
    return buffer.str();
}

/**
 * @brief Determines the MIME type based on the file extension.
 *
 * Returns a string representing the MIME type for common file extensions.
 * Defaults to "text/plain" if the extension is unrecognized.
 *
 * @param filepath The path to the file.
 * @return The MIME type as a string.
 */
std::string HttpResponse::getMimeType(const std::string& filepath) {
    if (filepath.rfind(".html") != std::string::npos) return "text/html";
    if (filepath.rfind(".css") != std::string::npos) return "text/css";
    if (filepath.rfind(".jpg") != std::string::npos || filepath.rfind(".jpeg") != std::string::npos) return "image/jpeg";
    if (filepath.rfind(".png") != std::string::npos) return "image/png";
    return "text/plain";
}

/**
 * @brief Generates an HTML directory listing for autoindex.
 *
 * Creates an HTML page listing all files and directories in the specified path.
 *
 * @param directory_path The filesystem path to the directory.
 * @param request_uri The URI path of the request (for display purposes).
 * @return HTML string containing the directory listing.
 */
std::string HttpResponse::generateDirectoryListing(const std::string& directory_path, const std::string& request_uri) {
    DIR* dir = opendir(directory_path.c_str());
    if (!dir) {
        return "";
    }

    std::stringstream html;
    html << "<!DOCTYPE html>\n<html>\n<head>\n";
    html << "<title>Index of " << request_uri << "</title>\n";
    html << "<style>body{font-family:Arial,sans-serif;margin:40px;}h1{border-bottom:1px solid #ccc;}a{text-decoration:none;display:block;padding:2px 0;}a:hover{background-color:#f0f0f0;}</style>\n";
    html << "</head>\n<body>\n";
    html << "<h1>Index of " << request_uri << "</h1>\n<hr>\n<pre>\n";

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        std::string name = entry->d_name;
        
        // Skip hidden files starting with '.'
        if (name[0] == '.') continue;

        std::string full_path = directory_path;
        if (full_path[full_path.length() - 1] != '/') {
            full_path += "/";
        }
        full_path += name;

        struct stat file_stat;
        if (stat(full_path.c_str(), &file_stat) == 0) {
            std::string link = request_uri;
            if (link[link.length() - 1] != '/') {
                link += "/";
            }
            link += name;

            if (S_ISDIR(file_stat.st_mode)) {
                html << "<a href=\"" << link << "/\">" << name << "/</a>\n";
            } else {
                html << "<a href=\"" << link << "\">" << name << "</a>\n";
            }
        }
    }
    closedir(dir);

    html << "</pre>\n<hr>\n</body>\n</html>";
    return html.str();
}

/**
 * @brief Builds the HTTP response header.
 *
 * Constructs the HTTP response header string with the given status code, status text,
 * content length, and content type.
 *
 * @param status_code The HTTP status code (e.g., 200, 404).
 * @param status_text The HTTP status text (e.g., "OK", "Not Found").
 * @param content_length The length of the response body in bytes.
 * @param content_type The MIME type of the response body.
 * @return The complete HTTP response header as a string.
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

// --- 4.0 HTTP Redirect Response Generator ---
/**
 * @brief Builds an HTTP redirect response.
 *
 * Generates a redirect response with the specified status code and location.
 *
 * @param status_code The HTTP status code (e.g., 301, 302).
 * @param location The redirect target URL.
 * @return The full HTTP redirect response as a string.
 */
std::string HttpResponse::buildRedirectResponse(int status_code, const std::string& location) {
    std::string status_text;
    if (status_code == 301) {
        status_text = "Moved Permanently";
    } else if (status_code == 302) {
        status_text = "Found";
    } else {
        status_text = "Redirect";
    }

    std::string body = "<html><head><title>Redirect</title></head><body><h1>" + status_text + "</h1><p>The resource has moved to <a href=\"" + location + "\">" + location + "</a>.</p></body></html>";
    
    std::stringstream ss;
    ss << "HTTP/1.1 " << status_code << " " << status_text << "\r\n";
    ss << "Location: " << location << "\r\n";
    ss << "Content-Type: text/html\r\n";
    ss << "Content-Length: " << body.length() << "\r\n";
    ss << "Connection: keep-alive\r\n";
    ss << "\r\n";
    return ss.str() + body;
}

// --- 4.1 Simple Error Response Generator ---
/**
 * @brief Builds a simple HTTP error response page.
 *
 * Generates a basic HTML error page for common HTTP error codes (404, 405, 403, 500).
 * Optionally uses the server configuration for custom error pages (not implemented).
 *
 * @param status_code The HTTP status code for the error.
 * @param server_config The server configuration (unused, for future custom error pages).
 * @return The full HTTP error response as a string.
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
 * @brief Finds the best matching server block for a given request.
 *
 * Searches the list of server configurations for one matching the client's port.
 * If none match, returns the first server config if available, otherwise nullptr.
 *
 * @param req The HTTP request.
 * @param configs The list of server configurations.
 * @param client_port The port number of the client connection.
 * @return Pointer to the matching ServerConfig, or nullptr if not found.
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
 * @brief Generates the HTTP response for a given request.
 *
 * Determines the appropriate server and location configuration, checks for redirects first,
 * then checks if the method is allowed, and dispatches to the correct handler (GET, POST, DELETE). 
 * Returns an error response if needed.
 *
 * @param req The HTTP request.
 * @param configs The list of server configurations.
 * @param client_port The port number of the client connection.
 * @return The full HTTP response as a string.
 */
std::string HttpResponse::generateResponse(const HttpRequest& req, const std::vector<ServerConfig>& configs, int client_port) {
    const ServerConfig* server_config = findMatchingServer(req, configs, client_port);
    if (!server_config) return buildErrorResponse(500, NULL);
    
    const LocationConfig* loc_config = findMatchingLocation(*server_config, req.getPath());
    if (!loc_config) return buildErrorResponse(404, server_config); 

    // Check for HTTP redirection first (highest priority)
    if (loc_config->return_code != 0) {
        return buildRedirectResponse(loc_config->return_code, loc_config->return_path);
    }

    // Check if method is allowed
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
 * @brief Handles DELETE requests by removing files from the filesystem.
 *
 * Attempts to delete the file at the resolved path. Returns 404 if not found, 403 if a directory,
 * 500 if deletion fails, or 204 No Content on success.
 *
 * @param loc_config The location configuration for the request.
 * @param uri The request URI.
 * @return The full HTTP response as a string.
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
 * @brief Handles POST requests by saving uploaded files to the server.
 *
 * If the target path is a directory, generates a unique filename. Writes the request body to the file.
 * Returns 201 Created on success, or an error response on failure.
 *
 * @param loc_config The location configuration for the request.
 * @param req The HTTP request containing the file data.
 * @return The full HTTP response as a string.
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