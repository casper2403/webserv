#include "../includes/HttpResponse.hpp"
#include <ctime>
#include <dirent.h>
#include <unistd.h>
#include <sys/wait.h>
#include <cstring>
#include <cstdlib>

// Helper to convert int to string
static std::string toString(int i) {
    std::stringstream ss; ss << i; return ss.str();
}

void HttpResponse::processRequest(Client& client, const std::vector<ServerConfig>& configs) {
    const HttpRequest& req = client.request;
    const ServerConfig* server_config = findMatchingServer(req, configs, client.listening_port);

    // 1. Check Payload Size
	std::cout << "Debug: Body Size=" << req.getBody().size() 
          << " Max=" << (server_config ? server_config->client_max_body_size : 0) << std::endl;
    if (server_config && req.getBody().size() > server_config->client_max_body_size) {
        client.response_buffer = buildErrorResponse(413, server_config);
        client.is_ready_to_write = true;
        return;
    }

    // 2. Routing
    const LocationConfig* loc_config = NULL;
    if (server_config)
        loc_config = findMatchingLocation(*server_config, req.getPath());
    
    if (!loc_config) {
        client.response_buffer = buildErrorResponse(404, server_config);
        client.is_ready_to_write = true;
        return;
    }

    // 3. Redirection
    if (loc_config->return_code != 0) {
        client.response_buffer = buildRedirectResponse(loc_config->return_code, loc_config->return_path);
        client.is_ready_to_write = true;
        return;
    }

    // 4. Method Allowed Check
    bool method_allowed = false;
    if (loc_config->methods.empty()) {
        if (req.getMethod() == "GET") method_allowed = true;
    } else {
        for (size_t i = 0; i < loc_config->methods.size(); ++i) {
            if (loc_config->methods[i] == req.getMethod()) { method_allowed = true; break; }
        }
    }
    if (!method_allowed) {
        client.response_buffer = buildErrorResponse(405, server_config);
        client.is_ready_to_write = true;
        return;
    }

    // 5. Determine File Path
    std::string request_path = req.getPath();
    size_t q_pos = request_path.find('?');
    if (q_pos != std::string::npos) request_path = request_path.substr(0, q_pos);

    std::string filepath = loc_config->root + request_path;
    struct stat st;
    if (stat(filepath.c_str(), &st) == 0 && S_ISDIR(st.st_mode) && !loc_config->index.empty()) {
        filepath += "/" + loc_config->index;
    }

    // 6. Handle CGI
    if (isCgiRequest(*loc_config, filepath)) {
        handleCgiRequest(client, *loc_config, filepath);
        return; // Return immediately (Async)
    }

    // 7. Handle Static
    std::string response;
    if (req.getMethod() == "GET") {
        response = handleGetRequest(*loc_config, req.getPath());
    } else if (req.getMethod() == "DELETE") {
        response = handleDeleteRequest(*loc_config, req.getPath());
    } else if (req.getMethod() == "POST") {
        response = handlePostRequest(*loc_config, req);
    } else {
        response = buildErrorResponse(501, server_config);
    }
    
    client.response_buffer = response;
    client.is_ready_to_write = true;
}

void HttpResponse::handleCgiRequest(Client& client, const LocationConfig& loc_config, const std::string& script_path) {
    (void)loc_config;
    const HttpRequest& req = client.request;
    
    // Setup Env
    std::vector<std::string> env_vars;
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
    if (!req.getHeader("Content-Length").empty())
        env_vars.push_back("CONTENT_LENGTH=" + req.getHeader("Content-Length"));
    env_vars.push_back("CONTENT_TYPE=" + req.getHeader("Content-Type"));
    env_vars.push_back("REDIRECT_STATUS=200");

    std::vector<char*> envp;
    for (size_t i = 0; i < env_vars.size(); ++i) envp.push_back(const_cast<char*>(env_vars[i].c_str()));
    envp.push_back(NULL);

    int pipe_in[2], pipe_out[2];
    if (pipe(pipe_in) == -1 || pipe(pipe_out) == -1) {
        client.response_buffer = buildErrorResponse(500, NULL);
        client.is_ready_to_write = true;
        return;
    }

    pid_t pid = fork();
    if (pid == -1) {
        close(pipe_in[0]); close(pipe_in[1]); close(pipe_out[0]); close(pipe_out[1]);
        client.response_buffer = buildErrorResponse(500, NULL);
        client.is_ready_to_write = true;
        return;
    }

    // ... inside the child process block ...
if (pid == 0) { // Child
    close(pipe_in[1]); close(pipe_out[0]);
    dup2(pipe_in[0], STDIN_FILENO);
    dup2(pipe_out[1], STDOUT_FILENO);
    close(pipe_in[0]); close(pipe_out[1]);

    char *argv[] = { const_cast<char*>(script_path.c_str()), NULL };
    
    // CHANGE STARTS HERE
    execve(script_path.c_str(), argv, envp.data());
    
    // If we reach here, execve failed!
    perror("execve failed");  // <--- Prints the exact error (e.g. Permission denied, No such file)
    std::cerr << "Failed to execute: " << script_path << std::endl;
    // CHANGE ENDS HERE
    
    exit(1);
} else { // Parent
        close(pipe_in[0]);
        close(pipe_out[1]);

        // Write Body to CGI (Simple blocking write for now)
        if (!req.getBody().empty()) {
            write(pipe_in[1], req.getBody().c_str(), req.getBody().size());
        }
        close(pipe_in[1]);

        // Set Client State for Async polling
        client.is_cgi_active = true;
        client.cgi_pid = pid;
        client.cgi_pipe_out = pipe_out[0]; // Read end
        client.cgi_output_buffer.clear();

		client.cgi_start_time = time(NULL);
    }
}

std::string HttpResponse::buildCgiResponse(const std::string& cgi_output) {
    size_t header_end = cgi_output.find("\r\n\r\n");
    if (header_end == std::string::npos) {
         return buildResponseHeader(200, "OK", cgi_output.length(), "text/plain") + cgi_output;
    }
    std::string cgi_headers = cgi_output.substr(0, header_end);
    std::string cgi_body = cgi_output.substr(header_end + 4);

    std::stringstream ss;
    ss << "HTTP/1.1 200 OK\r\n" << cgi_headers << "\r\nContent-Length: " << cgi_body.length() << "\r\n\r\n" << cgi_body;
    return ss.str();
}

std::string HttpResponse::buildErrorResponse(int status_code, const ServerConfig* server_config) {
    // Custom Error Page Check
    if (server_config && server_config->error_pages.count(status_code)) {
        std::string err_path = server_config->error_pages.at(status_code);
        if (err_path[0] != '/') err_path = server_config->root + "/" + err_path;
        
        std::string content = getFileContent(err_path);
        if (!content.empty()) {
            return buildResponseHeader(status_code, "Error", content.length(), "text/html") + content;
        }
    }
    
    // Default Fallback
    std::string body = "<html><body><h1>Error " + toString(status_code) + "</h1></body></html>";
    return buildResponseHeader(status_code, "Error", body.length(), "text/html") + body;
}

// --- KEEP EXISTING METHODS BELOW ---
const ServerConfig* HttpResponse::findMatchingServer(const HttpRequest& req, const std::vector<ServerConfig>& configs, int client_port) {
    (void)req;
    for (size_t i = 0; i < configs.size(); ++i) {
        if (configs[i].port == client_port) return &configs[i];
    }
    return configs.empty() ? NULL : &configs[0];
}

const LocationConfig* HttpResponse::findMatchingLocation(const ServerConfig& server, const std::string& path) {
    const LocationConfig* best_match = NULL;
    size_t best_match_length = 0;
    for (size_t i = 0; i < server.locations.size(); ++i) {
        if (path.find(server.locations[i].path) == 0) {
            if (server.locations[i].path.length() > best_match_length) {
                best_match = &server.locations[i];
                best_match_length = server.locations[i].path.length();
            }
        }
    }
    return best_match;
}

std::string HttpResponse::handleGetRequest(const LocationConfig& loc_config, const std::string& uri) {
    std::string filepath = (uri == "/") ? loc_config.root + "/" + loc_config.index : loc_config.root + uri;
    
    struct stat file_stat;
    if (stat(filepath.c_str(), &file_stat) != 0) return buildErrorResponse(404, NULL);

    if (S_ISREG(file_stat.st_mode)) {
        std::string content = getFileContent(filepath);
        return buildResponseHeader(200, "OK", content.length(), getMimeType(filepath)) + content;
    }
    if (S_ISDIR(file_stat.st_mode)) {
        if (loc_config.autoindex) {
             std::string listing = generateDirectoryListing(filepath, uri);
             return buildResponseHeader(200, "OK", listing.length(), "text/html") + listing;
        }
        return buildErrorResponse(403, NULL);
    }
    return buildErrorResponse(403, NULL);
}

std::string HttpResponse::handlePostRequest(const LocationConfig& loc_config, const HttpRequest& req) {
    std::string full_path = loc_config.root + req.getPath();
    std::ofstream outfile(full_path.c_str(), std::ios::binary);
    if (!outfile.is_open()) return buildErrorResponse(500, NULL);
    outfile << req.getBody();
    outfile.close();
    std::string body = "File created";
    return buildResponseHeader(201, "Created", body.length(), "text/plain") + body;
}

std::string HttpResponse::handleDeleteRequest(const LocationConfig& loc_config, const std::string& uri) {
    std::string filepath = loc_config.root + uri;
    if (std::remove(filepath.c_str()) != 0) return buildErrorResponse(500, NULL);
    return buildResponseHeader(204, "No Content", 0, "");
}

bool HttpResponse::isCgiRequest(const LocationConfig& loc_config, const std::string& path) {
    if (loc_config.cgi_ext.empty()) return false;
    size_t dot = path.rfind('.');
    if (dot == std::string::npos) return false;
    std::string ext = path.substr(dot);
    for(size_t i=0; i<loc_config.cgi_ext.size(); ++i) if (loc_config.cgi_ext[i] == ext) return true;
    return false;
}

std::string HttpResponse::getFileContent(const std::string& filepath) {
    std::ifstream ifs(filepath.c_str());
    if (!ifs.is_open()) return "";
    std::stringstream buffer; buffer << ifs.rdbuf(); return buffer.str();
}

std::string HttpResponse::getMimeType(const std::string& filepath) {
    if (filepath.rfind(".html") != std::string::npos) return "text/html";
    if (filepath.rfind(".css") != std::string::npos) return "text/css";
    return "text/plain";
}

std::string HttpResponse::generateDirectoryListing(const std::string& dir_path, const std::string& uri) {
    DIR* dir = opendir(dir_path.c_str());
    if (!dir) return "";
    std::stringstream ss;
    ss << "<html><body><h1>Index of " << uri << "</h1><hr><pre>";
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        ss << "<a href=\"" << entry->d_name << "\">" << entry->d_name << "</a><br>";
    }
    closedir(dir);
    ss << "</pre></body></html>";
    return ss.str();
}

std::string HttpResponse::buildResponseHeader(int status, const std::string& text, size_t len, const std::string& type) {
    std::stringstream ss;
    ss << "HTTP/1.1 " << status << " " << text << "\r\nContent-Type: " << type << "\r\nContent-Length: " << len << "\r\nConnection: keep-alive\r\n\r\n";
    return ss.str();
}

std::string HttpResponse::buildRedirectResponse(int status, const std::string& loc) {
    std::stringstream ss;
    ss << "HTTP/1.1 " << status << " Found\r\nLocation: " << loc << "\r\nContent-Length: 0\r\n\r\n";
    return ss.str();
}