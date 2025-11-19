#include "Config.hpp"
#include "Utils.hpp"
#include <fstream>
#include <sstream>
#include <iostream>

ConfigParser::ConfigParser(const std::string &path) {
    parse(path);
}

const std::vector<ServerConfig> &ConfigParser::servers() const {
    return _servers;
}

void ConfigParser::parse(const std::string &path) {
    std::ifstream file(path.c_str());
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open config file: " + path);
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    std::istringstream input(content);
    std::string word;

    while (input >> word) {
        if (word == "server") {
            std::string brace;
            input >> brace; // Expect "{"
            if (brace != "{") {
                throw std::runtime_error("Expected '{' after server");
            }
            ServerConfig server;
            while (true) {
                std::streampos pos = input.tellg();
                std::string key;
                if (!(input >> key)) {
                    throw std::runtime_error("Unterminated server block");
                }
                if (key == "}") {
                    break;
                }
                if (key == "listen") {
                    input >> server.port;
                    input >> key; // consume ;
                } else if (key == "root") {
                    input >> server.route.root;
                    input >> key;
                } else if (key == "autoindex") {
                    input >> word;
                    server.route.autoindex = (word == "on");
                    input >> key;
                } else if (key == "index") {
                    input >> server.route.index;
                    input >> key;
                } else {
                    std::ostringstream err;
                    err << "Unknown directive: " << key << " at position " << pos;
                    throw std::runtime_error(err.str());
                }
            }
            _servers.push_back(server);
        }
    }

    if (_servers.empty()) {
        ServerConfig defaultServer;
        _servers.push_back(defaultServer);
    }
}
