#include "Config.hpp"
#include "Server.hpp"
#include <iostream>

int main(int argc, char **argv) {
    std::string configPath = "config/webserv.conf";
    if (argc > 1) {
        configPath = argv[1];
    }
    try {
        ConfigParser parser(configPath);
        Server server(parser.servers());
        server.run();
    } catch (const std::exception &e) {
        std::cerr << "Fatal: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
