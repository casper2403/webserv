#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <vector>
#include <map>

struct ServerConfig {
    int port;
    std::string host;
    std::string root;
    std::vector<std::string> server_names;
    std::map<int, std::string> error_pages;
    // Add routes/locations here later
};

class ConfigParser {
public:
    // Returns a vector because one file can contain multiple "server { }" blocks
    std::vector<ServerConfig> parse(const std::string& filename);
};

#endif