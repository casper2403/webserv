/**
 * @file main.cpp
 * @brief Main entry point for the web server application.
 *
 * This file contains the main function that initializes and starts the web server.
 * It parses the configuration file, sets up the server, and runs it.
 */

#include "../includes/Webserver.hpp"
#include "../includes/Config.hpp" // Include your new parser

/**
 * @brief Main entry point for the web server application.
 *
 * This function parses the configuration file provided as a command-line argument,
 * initializes the web server with the parsed configuration, and starts the server loop.
 *
 * @param argc Argument count
 * @param argv Argument vector
 * @return int Exit status code (0 on success, 1 on error)
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