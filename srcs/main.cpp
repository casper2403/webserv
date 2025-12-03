#include "../includes/Webserver.hpp"
#include "../includes/Config.hpp" // Include your new parser

/**
 * @brief The main entry point for the web server application
 * 
 * @param argc Argument count
 * @param argv Argument vector
 * @return int Exit status
 */
int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: ./webserv [config_file]" << std::endl;
        return 1;
    }

    try {
        // 1. Parse the config file first
        ConfigParser parser;
        std::vector<ServerConfig> configs = parser.parse(argv[1]);

        // 2. Pass the configurations to the server
        Webserver server;
        server.init(configs); 
        server.run();

    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    return 0;
}