#include "../includes/Config.hpp"
#include <cstdlib> // for atoi

// Helper to remove semicolons from end of strings
static std::string trim(const std::string& str) {
    std::string s = str;
    if (!s.empty() && s[s.length() - 1] == ';') {
        s.erase(s.length() - 1);
    }
    return s;
}

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
        // Add more directives (error_page, client_max_body_size) here
    }
    throw std::runtime_error("Error: Unexpected end of file inside server block");
}

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
        }
        // Add methods, return, etc. here
    }
    throw std::runtime_error("Error: Unexpected end of file inside location block");
}