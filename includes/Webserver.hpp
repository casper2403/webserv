#ifndef WEBSERVER_HPP
#define WEBSERVER_HPP

#include "Config.hpp"
#include <vector>
#include <poll.h>
#include <map>
#include <string>
#include <iostream>
#include <netinet/in.h> // For sockaddr_in
#include <sys/socket.h>
#include <fcntl.h>	// For fcntl
#include <unistd.h> // For close
#include <cstring>	// For memset
#include <cstdio>	// For perror
#include <cstdlib>	// For exit
#include "HttpRequest.hpp"

struct Client
{
    int fd;
    HttpRequest request;
    std::string response_buffer;
    bool is_ready_to_write;
    int listening_port;
};

class Webserver
{
private:
    std::vector<struct pollfd> _fds;
    std::vector<int> _server_fds;
    std::map<int, Client> _clients;

	void initSocket(int port);
	void acceptConnection(int server_fd);
	void handleClientData(int client_fd);
	void handleClientRead(int client_fd);
	void handleClientWrite(int client_fd);

	int getServerPort(int server_fd); // New helper to get port from server fd
	std::map<int, int> _server_fd_to_port; // Maps listening FD -> Port
    const std::vector<ServerConfig>* _configs_ptr; // Stores pointer to config vector

public:
	Webserver();
	~Webserver();

	// Parse config and setup sockets
	void init(const std::vector<ServerConfig>& configs);
	// The main loop
	void run();
};

#endif