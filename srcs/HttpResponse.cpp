#include "../includes/HttpResponse.hpp"
#include <ctime>
#include <dirent.h>
#include <unistd.h>
#include <sys/wait.h>
#include <cstring>
#include <cstdlib>

/**
 * @brief Checks if the requested path corresponds to a CGI script defined in the config.
 */
bool HttpResponse::isCgiRequest(const LocationConfig& loc_config, const std::string& path) {
    if (loc_config.cgi_ext.empty()) return false;
    
    size_t dot_pos = path.rfind('.');
    if (dot_pos == std::string::npos) return false;
    
    std::string ext = path.substr(dot_pos);
    for (size_t i = 0; i < loc_config.cgi_ext.size(); ++i) {
        if (loc_config.cgi_ext[i] == ext) return true;
    }
    return false;
}

/**
 * @brief Handles CGI execution.
 */
std::string HttpResponse::handleCgiRequest(const LocationConfig& loc_config, const HttpRequest& req, const std::string& script_path) {
    (void)loc_config; // Unused for now, but available if needed

    // 1. Prepare Environment Variables
    std::vector<std::string> env_vars;
    
    // Extract Query String
    std::string uri = req.getPath();
    std::string query_string = "";
    size_t q_pos = uri.find('?');
    if (q_pos != std::string::npos) {
        query_string = uri.substr(q_pos + 1);
        uri = uri.substr(0, q_pos);
    }

    env_vars.push_back("REQUEST_METHOD=" + req.getMethod());
    env_vars.push_back("QUERY_STRING=" + query_string);
    env_vars.push_back("SCRIPT_FILENAME=" + script_path);
    env_vars.push_back("PATH_INFO=" + uri);
    env_vars.push_back("SERVER_PROTOCOL=HTTP/1.1");
    env_vars.push_back("CONTENT_LENGTH=" + req.getHeader("Content-Length"));
    env_vars.push_back("CONTENT_TYPE=" + req.getHeader("Content-Type"));
    env_vars.push_back("REDIRECT_STATUS=200"); // Needed for PHP-CGI

    std::vector<char*> envp;
    for (size_t i = 0; i < env_vars.size(); ++i) {
        envp.push_back(const_cast<char*>(env_vars[i].c_str()));
    }
    envp.push_back(NULL);

    // 2. Setup Pipes
    int pipe_in[2];  // Parent writes -> Child reads (stdin)
    int pipe_out[2]; // Child writes -> Parent reads (stdout)
    
    if (pipe(pipe_in) == -1 || pipe(pipe_out) == -1) {
        return buildErrorResponse(500, NULL);
    }

    pid_t pid = fork();
    if (pid == -1) {
        close(pipe_in[0]); close(pipe_in[1]);
        close(pipe_out[0]); close(pipe_out[1]);
        return buildErrorResponse(500, NULL);
    }

    if (pid == 0) { // Child Process
        close(pipe_in[1]);
        close(pipe_out[0]);

        dup2(pipe_in[0], STDIN_FILENO);
        dup2(pipe_out[1], STDOUT_FILENO);
        
        close(pipe_in[0]);
        close(pipe_out[1]);

        char *argv[] = { const_cast<char*>(script_path.c_str()), NULL };
        execve(script_path.c_str(), argv, envp.data());
        
        exit(1);
    } 
    else { // Parent Process
        close(pipe_in[0]);
        close(pipe_out[1]);

        // Write Request Body to CGI (for POST)
        if (!req.getBody().empty()) {
            write(pipe_in[1], req.getBody().c_str(), req.getBody().size());
        }
        close(pipe_in[1]);

        // Read Output
        char buffer[4096];
        std::string cgi_output;
        int bytes_read;
        while ((bytes_read = read(pipe_out[0], buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytes_read] = '\0';
            cgi_output += buffer;
        }
        close(pipe_out[0]);
        
        int status;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            return buildErrorResponse(502, NULL);
        }

        // Parse CGI Output
        size_t header_end = cgi_output.find("\r\n\r\n");
        if (header_end == std::string::npos) {
             return buildResponseHeader(200, "OK", cgi_output.length(), "text/plain") + cgi_output;
        }

        std::string cgi_headers = cgi_output.substr(0, header_end);
        std::string cgi_body = cgi_output.substr(header_end + 4);

        std::stringstream ss;
        ss << "HTTP/1.1 200 OK\r\n";
        ss << cgi_headers << "\r\n";
        ss << "Content-Length: " << cgi_body.length() << "\r\n";
        ss << "\r\n";
        ss << cgi_body;
        
        return ss.str();
    }
}

// --- Rest of the existing methods ---

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

std::string HttpResponse::handleGetRequest(const LocationConfig& loc_config, const std::string& uri) {
    std::string filepath;
    if (uri == "/") {
        filepath = loc_config.root + "/" + loc_config.index; // Naive join, but works for basic cases
    } else {
        filepath = loc_config.root + uri;
    }
    // Clean up double slashes if any
    size_t dslash;
    while ((dslash = filepath.find("//")) != std::string::npos) {
        filepath.replace(dslash, 2, "/");
    }

    struct stat file_stat;
    if (stat(filepath.c_str(), &file_stat) != 0) {
        return buildErrorResponse(404, NULL);
    }

    if (S_ISREG(file_stat.st_mode)) {
        std::string content = getFileContent(filepath);
        std::string mime_type = getMimeType(filepath);
        return buildResponseHeader(200, "OK", content.length(), mime_type) + content;
    }

    if (S_ISDIR(file_stat.st_mode)) {
        std::string dir_path = filepath;
        if (dir_path[dir_path.length() - 1] != '/') dir_path += "/";

        if (!loc_config.index.empty()) {
            std::string index_path = dir_path + loc_config.index;
            struct stat index_stat;
            if (stat(index_path.c_str(), &index_stat) == 0 && S_ISREG(index_stat.st_mode)) {
                std::string content = getFileContent(index_path);
                std::string mime_type = getMimeType(index_path);
                return buildResponseHeader(200, "OK", content.length(), mime_type) + content;
            }
        }

        if (loc_config.autoindex) {
            std::string listing = generateDirectoryListing(dir_path, uri);
            if (!listing.empty()) {
                return buildResponseHeader(200, "OK", listing.length(), "text/html") + listing;
            }
        }
        return buildErrorResponse(403, NULL);
    }
    return buildErrorResponse(403, NULL);
}

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

std::string HttpResponse::handlePostRequest(const LocationConfig& loc_config, const HttpRequest& req) {
    std::string full_path = loc_config.root + req.getPath();
    
    struct stat st;
    if (stat(full_path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
        if (full_path[full_path.length() - 1] != '/') full_path += "/";
        std::stringstream ss;
        ss << "upload_" << std::time(NULL) << ".dat";
        full_path += ss.str();
    }

    std::ofstream outfile(full_path.c_str(), std::ios::binary);
    if (!outfile.is_open()) {
        return buildErrorResponse(500, NULL);
    }
    outfile << req.getBody();
    outfile.close();

    std::string body = "File created successfully: " + full_path;
    return buildResponseHeader(201, "Created", body.length(), "text/plain") + body;
}

std::string HttpResponse::getFileContent(const std::string& filepath) {
    std::ifstream ifs(filepath.c_str());
    std::stringstream buffer;
    buffer << ifs.rdbuf();
    return buffer.str();
}

std::string HttpResponse::getMimeType(const std::string& filepath) {
    if (filepath.rfind(".html") != std::string::npos) return "text/html";
    if (filepath.rfind(".css") != std::string::npos) return "text/css";
    if (filepath.rfind(".jpg") != std::string::npos) return "image/jpeg";
    if (filepath.rfind(".png") != std::string::npos) return "image/png";
    return "text/plain";
}

std::string HttpResponse::generateDirectoryListing(const std::string& directory_path, const std::string& request_uri) {
    DIR* dir = opendir(directory_path.c_str());
    if (!dir) return "";

    std::stringstream html;
    html << "<!DOCTYPE html><html><body><h1>Index of " << request_uri << "</h1><hr><pre>";
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        std::string name = entry->d_name;
        if (name[0] == '.') continue;
        html << "<a href=\"" << name << "\">" << name << "</a><br>";
    }
    closedir(dir);
    html << "</pre></body></html>";
    return html.str();
}

std::string HttpResponse::buildResponseHeader(int status_code, const std::string& status_text, size_t content_length, const std::string& content_type) {
    std::stringstream ss;
    ss << "HTTP/1.1 " << status_code << " " << status_text << "\r\n";
    ss << "Content-Type: " << content_type << "\r\n";
    ss << "Content-Length: " << content_length << "\r\n";
    ss << "Connection: keep-alive\r\n\r\n";
    return ss.str();
}

std::string HttpResponse::buildRedirectResponse(int status_code, const std::string& location) {
    std::stringstream ss;
    ss << "HTTP/1.1 " << status_code << " Found\r\n";
    ss << "Location: " << location << "\r\n";
    ss << "Content-Length: 0\r\n\r\n";
    return ss.str();
}

std::string HttpResponse::buildErrorResponse(int status_code, const ServerConfig* server_config) {
    (void)server_config;
    std::stringstream ss;
    ss << "HTTP/1.1 " << status_code << " Error\r\nContent-Length: 0\r\n\r\n";
    return ss.str();
}

const ServerConfig* HttpResponse::findMatchingServer(const HttpRequest& req, const std::vector<ServerConfig>& configs, int client_port) {
    (void)req;
    for (size_t i = 0; i < configs.size(); ++i) {
        if (configs[i].port == client_port) return &configs[i];
    }
    return configs.empty() ? NULL : &configs[0];
}

std::string HttpResponse::generateResponse(const HttpRequest& req, const std::vector<ServerConfig>& configs, int client_port) {
    const ServerConfig* server_config = findMatchingServer(req, configs, client_port);
    if (!server_config) return buildErrorResponse(500, NULL);
    
    const LocationConfig* loc_config = findMatchingLocation(*server_config, req.getPath());
    if (!loc_config) return buildErrorResponse(404, server_config); 

    if (loc_config->return_code != 0) {
        return buildRedirectResponse(loc_config->return_code, loc_config->return_path);
    }

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

    // CGI CHECK
    std::string request_path = req.getPath();
    size_t q_pos = request_path.find('?');
    if (q_pos != std::string::npos) request_path = request_path.substr(0, q_pos);

    std::string filepath = loc_config->root + request_path;
    // Basic fix for root path mapped to index
    struct stat st;
    if (stat(filepath.c_str(), &st) == 0 && S_ISDIR(st.st_mode) && !loc_config->index.empty()) {
        filepath += "/" + loc_config->index;
    }

    if (isCgiRequest(*loc_config, filepath)) {
         return handleCgiRequest(*loc_config, req, filepath);
    }

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