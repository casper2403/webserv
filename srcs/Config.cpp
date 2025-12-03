#include "../includes/Config.hpp"
#include <cstdlib> // for atoi
#include <algorithm> // for std::find

// Helper to remove semicolons from end of strings
/**
 * @brief Trim semicolons from the end of a string
 * 
 * @param str The input string
 * @return std::string The trimmed string
 */
static std::string trim(const std::string& str) {
    std::string s = str;
    if (!s.empty() && s[s.length() - 1] == ';') {
        s.erase(s.length() - 1);
    }
    return s;
}

/**
 * @brief Parse the configuration file and return server configurations
 * 
 * @param filename Path to the configuration file
 * @return std::vector<ServerConfig> Vector of parsed server configurations
 */
std::vector<ServerConfig> ConfigParser::parse(const std::string& filename) {
    std::ifstream file(filename.c_str());
    if (!file.is_open()) {
        throw std::runtime_error("Error: Could not open config file");
    }

    std::stringstream buffer;
    buffer << file.rdbuf(); // Read whole file into stringstream

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
            servers.push_back(server);
        } else {
            throw std::runtime_error("Error: Unexpected token '" + token + "' in global scope");
        }
    }
    return servers;
}

/**
 * @brief Parse a server block from the configuration
 * 
 * @param ss String stream containing the config data
 * @param config ServerConfig object to populate
 */
void ConfigParser::parseServerBlock(std::stringstream& ss, ServerConfig& config) {
    std::string token;
    while (ss >> token) {
        if (token == "}") return; // End of server block

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
            // Syntax: error_page 404 /404.html;
            int code;
            std::string path;
            ss >> code >> path;
            config.error_pages[code] = trim(path);
        } else if (token == "client_max_body_size") {
            // Syntax: client_max_body_size 10M;
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
 * @brief Parse a location block from the configuration
 * 
 * @param ss String stream containing the config data
 * @param loc LocationConfig object to populate
 */
void ConfigParser::parseLocationBlock(std::stringstream& ss, LocationConfig& loc) {
    std::string token;
    while (ss >> token) {
        if (token == "}") return; // End of location block

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
            // Syntax: allow_methods GET POST DELETE;
            // Loop until we hit a token that contains ';'
            std::string method;
            while (ss >> method) {
                std::string cleanMethod = trim(method);
                if (isValidMethod(cleanMethod)) {
                    loc.methods.push_back(cleanMethod);
                }
                if (method.find(';') != std::string::npos) break;
            }
        } else if (token == "return") {
            // Syntax: return 301 /newpath;
            ss >> loc.return_code;
            ss >> loc.return_path;
            loc.return_path = trim(loc.return_path);
        }
    }
}

/**
 * @brief Check if the given method is valid
 * 
 * @param method HTTP method string
 * @return true if valid, false otherwise
 */
bool ConfigParser::isValidMethod(const std::string& method) {
    return (method == "GET" || method == "POST" || method == "DELETE");
}