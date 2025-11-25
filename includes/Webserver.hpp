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

// A simple structure to hold client state
struct Client
{
    int fd;
    std::string request_buffer;  // Stores incoming data (recv)
    std::string response_buffer; // Stores outgoing data (send)
    bool is_ready_to_write;      // Logic flag: Do we have a complete response ready?
};

class Webserver
{
private:
	// Vector of all FDs to monitor (listeners + clients)
	std::vector<struct pollfd> _fds;

	// Map to keep track of client data associated with an FD
	std::map<int, Client> _clients;

	void initSocket(int port);
	void acceptConnection(int server_fd);
	void handleClientData(int client_fd);
	void handleClientRead(int client_fd);
	void handleClientWrite(int client_fd);

public:
	Webserver();
	~Webserver();

	// Parse config and setup sockets
void init(const std::vector<ServerConfig>& configs);
	// The main loop
	void run();
};

#endif