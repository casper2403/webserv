#include "../includes/Webserver.hpp"
#include <iostream>

int main(int argc, char **argv) {
    if (argc != 2) {
        // "Your program must use a configuration file provided as an argument" [cite: 112]
        std::cerr << "Usage: ./webserv [config_file]" << std::endl;
        return 1;
    }

    try {
        Webserver server;
        server.init(argv[1]);
        server.run();
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    return 0;
}