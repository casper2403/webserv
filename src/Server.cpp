#include "Server.hpp"
#include "Utils.hpp"
#include <stdexcept>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>

Server::Server(const std::vector<ServerConfig> &configs) : _configs(configs) {
    setupSockets();
}

void Server::setupSockets() {
    for (size_t i = 0; i < _configs.size(); ++i) {
        addListeningSocket(_configs[i].port);
    }
}

void Server::addListeningSocket(int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        throw std::runtime_error("Failed to create socket");
    }

    int opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        close(sock);
        throw std::runtime_error("setsockopt failed");
    }

    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(sock, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0) {
        close(sock);
        throw std::runtime_error("bind failed");
    }

    if (listen(sock, 128) < 0) {
        close(sock);
        throw std::runtime_error("listen failed");
    }

    _listenSockets.push_back(sock);
}

void Server::run() {
    std::cout << "Webserv listening on:";
    for (size_t i = 0; i < _configs.size(); ++i) {
        std::cout << " " << _configs[i].port;
    }
    std::cout << std::endl;

    while (true) {
        std::vector<struct pollfd> fds;
        for (size_t i = 0; i < _listenSockets.size(); ++i) {
            struct pollfd p;
            p.fd = _listenSockets[i];
            p.events = POLLIN;
            p.revents = 0;
            fds.push_back(p);
        }
        for (size_t i = 0; i < _clients.size(); ++i) {
            short events = POLLIN;
            if (_clients[i].hasResponse()) {
                events |= POLLOUT;
            }
            fds.push_back(_clients[i].toPollfd(events));
        }

        int ret = poll(&fds[0], fds.size(), 1000);
        if (ret < 0) {
            throw std::runtime_error("poll failed");
        }

        size_t index = 0;
        for (size_t i = 0; i < _listenSockets.size(); ++i, ++index) {
            if (fds[index].revents & POLLIN) {
                acceptConnection(i);
            }
        }

        for (size_t i = 0; i < _clients.size(); ++i, ++index) {
            if (_clients[i].isClosed()) {
                continue;
            }
            if (fds[index].revents & POLLIN) {
                handleClient(i);
            }
            if (fds[index].revents & POLLOUT && _clients[i].hasResponse()) {
                const std::string &res = _clients[i].response().raw();
                ssize_t sent = send(_clients[i].fd(), res.c_str(), res.size(), 0);
                if (sent < 0) {
                    _clients[i].setClosed();
                } else {
                    _clients[i].setClosed();
                }
            }
        }

        std::vector<Connection> alive;
        for (size_t i = 0; i < _clients.size(); ++i) {
            if (!_clients[i].isClosed()) {
                alive.push_back(_clients[i]);
            } else {
                close(_clients[i].fd());
            }
        }
        _clients.swap(alive);
    }
}

void Server::acceptConnection(size_t index) {
    (void)index;
    struct sockaddr_in client;
    socklen_t len = sizeof(client);
    int fd = accept(_listenSockets[index], reinterpret_cast<struct sockaddr *>(&client), &len);
    if (fd < 0) {
        return;
    }
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    _clients.push_back(Connection(fd));
}

void Server::handleClient(size_t index) {
    char buffer[4096];
    ssize_t bytes = recv(_clients[index].fd(), buffer, sizeof(buffer), 0);
    if (bytes <= 0) {
        _clients[index].setClosed();
        return;
    }
    _clients[index].appendRequest(buffer, bytes);
    if (_clients[index].parseRequest()) {
        HttpRequest &req = _clients[index].request();
        HttpResponse &res = _clients[index].response();
        res.reset();
        std::string path = req.path();
        if (path == "/") {
            path = "/" + _configs[0].route.index;
        }
        std::string full = joinPath(_configs[0].route.root, path);
        if (isDirectory(full)) {
            if (_configs[0].route.autoindex) {
                res.setStatus(200, "OK");
                res.setHeader("Content-Type", "text/plain");
                res.setBody("Autoindex is enabled, but listing is not implemented.\n");
            } else {
                res.setStatus(403, "Forbidden");
                res.setHeader("Content-Type", "text/plain");
                res.setBody("Directory listing not allowed\n");
            }
        } else if (fileExists(full)) {
            try {
                std::string body = readFile(full);
                res.setStatus(200, "OK");
                res.setHeader("Content-Type", mimeType(full));
                res.setBody(body);
            } catch (const std::exception &e) {
                res.setStatus(500, "Internal Server Error");
                res.setHeader("Content-Type", "text/plain");
                res.setBody(e.what());
            }
        } else {
            res.setStatus(404, "Not Found");
            res.setHeader("Content-Type", "text/plain");
            res.setBody("Not Found\n");
        }
        res.setHeader("Connection", "close");
    }
}
