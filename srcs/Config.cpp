#include "../includes/Config.hpp"

std::vector<ServerConfig> ConfigParser::parse(const std::string& filename) {
    (void)filename; // Unused for now
    
    std::vector<ServerConfig> servers;
    
    // Mock data to test your logic
    ServerConfig server1;
    server1.port = 8080;
    server1.host = "127.0.0.1";
    servers.push_back(server1);
    
    return servers;
}