#include "../includes/Config.hpp"
#include <cstdlib> // for atoi
#include <algorithm> // for std::find

/**
 * @brief Removes a trailing semicolon from a string, if present.
 */
static std::string trim(const std::string& str) {
    std::string s = str;
    if (!s.empty() && s[s.length() - 1] == ';') {
        s.erase(s.length() - 1);
    }
    return s;
}

/**
 * @brief Checks if the given HTTP method is valid (GET, POST, DELETE).
 */
bool ConfigParser::isValidMethod(const std::string& method) {
    return (method == "GET" || method == "POST" || method == "DELETE");
}

/**
 * @brief Parses the configuration file and returns a vector of server configurations.
 */
std::vector<ServerConfig> ConfigParser::parse(const std::string& filename) {
    std::ifstream file(filename.c_str());
    if (!file.is_open()) {
        throw std::runtime_error("Error: Could not open config file");
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    std::vector<ServerConfig> servers;
    std::string token;

    while (buffer >> token) {
        if (token == "server") {
            std::string brace;
            buffer >> brace;
            if (brace != "{") {
                throw std::runtime_error("Error: Expected '{' after server");
            }
            
            ServerConfig server;
            parseServerBlock(buffer, server);

            // --- START FIX ---
            // Post-processing: Inherit root from server if location root is empty
            for (size_t i = 0; i < server.locations.size(); ++i) {
                if (server.locations[i].root.empty()) {
                    server.locations[i].root = server.root;
                }
            }
            // --- END FIX ---

            servers.push_back(server);
        } else {
            throw std::runtime_error("Error: Unexpected token '" + token + "' in global scope");
        }
    }
    return servers;
}

/**
 * @brief Parses a server block from the configuration stream.
 */
void ConfigParser::parseServerBlock(std::stringstream& ss, ServerConfig& config) {
    std::string token;
    while (ss >> token) {
        if (token == "}") return;

        if (token == "listen") {
            std::string portStr;
            ss >> portStr;
            config.port = std::atoi(trim(portStr).c_str());
        } else if (token == "host") {
            ss >> config.host;
            config.host = trim(config.host);
        } else if (token == "server_name") {
            std::string name;
            ss >> name;
            config.server_names.push_back(trim(name));
        } else if (token == "root") {
            ss >> config.root;
            config.root = trim(config.root);
        } else if (token == "error_page") {
            int code;
            std::string path;
            ss >> code >> path;
            config.error_pages[code] = trim(path);
        } else if (token == "client_max_body_size") {
            std::string sizeStr;
            ss >> sizeStr;
            sizeStr = trim(sizeStr);
            unsigned long size = std::atol(sizeStr.c_str());
            char unit = sizeStr[sizeStr.size() - 1];
            if (unit == 'K' || unit == 'k') size *= 1024;
            else if (unit == 'M' || unit == 'm') size *= 1024 * 1024;
            else if (unit == 'G' || unit == 'g') size *= 1024 * 1024 * 1024;
            config.client_max_body_size = size;
        } else if (token == "location") {
            std::string path;
            ss >> path;
            LocationConfig loc;
            loc.path = path;
            
            std::string brace;
            ss >> brace; // Expect '{'
            
            parseLocationBlock(ss, loc);
            config.locations.push_back(loc);
        }
    }
    throw std::runtime_error("Error: Unexpected end of file inside server block");
}

/**
 * @brief Parses a location block from the configuration stream.
 */
void ConfigParser::parseLocationBlock(std::stringstream& ss, LocationConfig& loc) {
    std::string token;
    while (ss >> token) {
        if (token == "}") return; 

        if (token == "root") {
            ss >> loc.root;
            loc.root = trim(loc.root);
        } else if (token == "index") {
            ss >> loc.index;
            loc.index = trim(loc.index);
        } else if (token == "autoindex") {
            std::string val;
            ss >> val;
            loc.autoindex = (trim(val) == "on");
        } else if (token == "allow_methods") {
            std::string method;
            while (ss >> method) {
                std::string cleanMethod = trim(method);
                if (isValidMethod(cleanMethod)) {
                    loc.methods.push_back(cleanMethod);
                }
                if (method.find(';') != std::string::npos) break;
            }
        } else if (token == "return") {
            ss >> loc.return_code;
            ss >> loc.return_path;
            loc.return_path = trim(loc.return_path);
        } else if (token == "cgi_ext") { // Correctly placed inside parseLocationBlock
            std::string ext;
            while (ss >> ext) {
                std::string cleanExt = trim(ext);
                loc.cgi_ext.push_back(cleanExt);
                if (ext.find(';') != std::string::npos) break;
            }
        }
    }
}