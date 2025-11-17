#ifndef WEBSERVER_HPP
#define WEBSERVER_HPP

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
	std::string request_buffer;
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

public:
	Webserver();
	~Webserver();

	// Parse config and setup sockets
	void init(const char *config_path);

	// The main loop
	void run();
};

#endif