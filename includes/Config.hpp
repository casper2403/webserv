#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>

struct LocationConfig {
    std::string path;
    std::string root;
    std::string index;
    bool autoindex;
    std::vector<std::string> methods; // GET, POST, DELETE
    std::string return_path; // For redirections
    // Add upload paths, cgi extensions etc. later

    LocationConfig() : autoindex(false) {}
};

struct ServerConfig {
    int port;
    std::string host;
    std::string root;
    std::vector<std::string> server_names;
    std::map<int, std::string> error_pages;
    unsigned long client_max_body_size;
    std::vector<LocationConfig> locations;

    ServerConfig() : port(80), host("0.0.0.0"), root("./"), client_max_body_size(1024 * 1024) {}
};

class ConfigParser {
public:
    std::vector<ServerConfig> parse(const std::string& filename);

private:
    void parseServerBlock(std::stringstream& ss, ServerConfig& config);
    void parseLocationBlock(std::stringstream& ss, LocationConfig& location);
};

#endif