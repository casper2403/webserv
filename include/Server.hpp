#ifndef SERVER_HPP
#define SERVER_HPP

#include "Config.hpp"
#include "Connection.hpp"
#include <vector>
#include <poll.h>

class Server {
public:
    explicit Server(const std::vector<ServerConfig> &configs);
    void run();

private:
    std::vector<int> _listenSockets;
    std::vector<ServerConfig> _configs;
    std::vector<Connection> _clients;

    void setupSockets();
    void addListeningSocket(int port);
    void acceptConnection(size_t index);
    void handleClient(size_t index);
};

#endif
