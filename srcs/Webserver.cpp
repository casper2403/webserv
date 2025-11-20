#include "../includes/Webserver.hpp"

Webserver::Webserver() {}
Webserver::~Webserver() {}

void Webserver::init(const char *config_path)
{
	(void)config_path;
	// TODO: Write your Config Parser here later.
	// For now, we hardcode listening on port 8080.
	initSocket(8080);
	std::cout << "Server initialized on port 8080" << std::endl;
}

// Setup the listening socket
void Webserver::initSocket(int port)
{
	int server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd < 0)
	{
		perror("socket failed");
		exit(EXIT_FAILURE);
	}

	// Allow socket descriptor to be reusable immediately
	int opt = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
	{
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}

	// Set socket to NON-BLOCKING mode (Critical for 42 Project)
	// [cite: 114, 145]
	if (fcntl(server_fd, F_SETFL, O_NONBLOCK) < 0)
	{
		perror("fcntl");
		exit(EXIT_FAILURE);
	}

	struct sockaddr_in address;
	memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(port);

	if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
	{
		perror("bind failed");
		exit(EXIT_FAILURE);
	}

	if (listen(server_fd, 10) < 0)
	{
		perror("listen");
		exit(EXIT_FAILURE);
	}

	// Add the listening socket to our pollfd vector
	struct pollfd pfd;
	pfd.fd = server_fd;
	pfd.events = POLLIN; // We are interested in reading (new connections)
	pfd.revents = 0;
	_fds.push_back(pfd);
}

void Webserver::run()
{
	std::cout << "Waiting for connections..." << std::endl;

	while (true)
	{
		// Call poll() - this blocks until an event happens
		//
		int ret = poll(&_fds[0], _fds.size(), -1);

		if (ret < 0)
		{
			perror("poll");
			break;
		}

		// Iterate over FDs to check for events
		// CAUTION: We use a standard loop index because modifying _fds
		// inside the loop (e.g. removing a client) invalidates iterators.
		for (size_t i = 0; i < _fds.size(); ++i)
		{

			// Check if we have data to read
			if (_fds[i].revents & POLLIN)
			{

				// If the FD is our listening socket, it's a NEW connection
				// (For simplicity, assuming index 0 is listener.
				// In real project, check against list of listener FDs).
				if (_fds[i].fd == _fds[0].fd)
				{
					acceptConnection(_fds[i].fd);
				}
				else
				{
					// It's an existing client sending data
					handleClientData(_fds[i].fd);
				}
			}
		}
	}
}

void Webserver::acceptConnection(int server_fd)
{
	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);

	int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
	if (client_fd < 0)
	{
		perror("accept");
		return;
	}

	// Set new client to NON-BLOCKING mode
	if (fcntl(client_fd, F_SETFL, O_NONBLOCK) < 0)
	{
		perror("fcntl client");
		close(client_fd);
		return;
	}

	// Add to poll vector
	struct pollfd pfd;
	pfd.fd = client_fd;
	pfd.events = POLLIN | POLLOUT; // Check for Read AND Write readiness
	pfd.revents = 0;
	_fds.push_back(pfd);

	// Add to map
	Client new_client;
	new_client.fd = client_fd;
	_clients[client_fd] = new_client;

	std::cout << "New connection: " << client_fd << std::endl;
}

void Webserver::handleClientData(int client_fd)
{
	char buffer[1024];
	int bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

	if (bytes_read <= 0)
	{
		// Client disconnected or error
		if (bytes_read == 0)
			std::cout << "Client " << client_fd << " disconnected" << std::endl;
		else
			perror("recv");

		close(client_fd);

		// Remove from _fds vector (Manual loop required to find it)
		for (std::vector<struct pollfd>::iterator it = _fds.begin(); it != _fds.end(); ++it)
		{
			if (it->fd == client_fd)
			{
				_fds.erase(it);
				break;
			}
		}
		_clients.erase(client_fd);
	}
	else
	{
		buffer[bytes_read] = '\0';
		std::cout << "Received data from " << client_fd << ":\n"
				  << buffer << std::endl;

		// --- SUPER BASIC RESPONSE ---
		// In the real project, you never write immediately.
		// You add to a buffer and wait for POLLOUT.
		// This is just to test "Hello World".
		std::string response = "HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\nHello World!\n";
		send(client_fd, response.c_str(), response.size(), 0);
	}
}