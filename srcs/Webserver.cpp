#include "../includes/Webserver.hpp"
#include "../includes/Config.hpp" // Include your new parser
#include "../includes/HttpResponse.hpp"

Webserver::Webserver() {}
Webserver::~Webserver() {}

/**
 * @brief Initialize the webserver by setting up listening sockets for each unique port in the configs.
 * @param configs Vector of ServerConfig objects containing server settings.s
 */
void Webserver::init(const std::vector<ServerConfig> &configs)
{
	std::vector<int> listening_ports;
	_configs_ptr = &configs;

	for (size_t i = 0; i < configs.size(); ++i)
	{
		int port = configs[i].port;

		bool port_exists = false;
		for (size_t j = 0; j < listening_ports.size(); ++j)
		{
			if (listening_ports[j] == port)
			{
				port_exists = true;
				break;
			}
		}

		// If not, open the socket and add it to our list
		if (!port_exists)
		{
			initSocket(port);
			listening_ports.push_back(port);
			std::cout << "Server initialized on port " << port << std::endl;
		}
	}
}

// Setup the listening socket
/**
 * @brief Initialize the listening socket for a specific port.
 * @param port The port number to listen on.
 */
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
		close(server_fd);
		exit(EXIT_FAILURE);
	}

	// Add the listening socket to our pollfd vector
	struct pollfd pfd;
	pfd.fd = server_fd;
	pfd.events = POLLIN; // We are interested in reading (new connections)
	pfd.revents = 0;
	_fds.push_back(pfd);
	_server_fds.push_back(server_fd);
	_server_fd_to_port[server_fd] = port;
}

/**
 * @brief Run the webserver event loop.
 */
void Webserver::run()
{
	std::cout << "Waiting for connections..." << std::endl;

	while (true)
	{
		int ret = poll(&_fds[0], _fds.size(), -1);

		// FIX: Check the return value to resolve the "unused variable" error
		if (ret < 0)
		{
			perror("poll");
			break;
		}

		// Iterate over FDs to check for events
		for (size_t i = 0; i < _fds.size(); ++i)
		{
			// 1. HANDLE READ (Recv)
			if (_fds[i].revents & POLLIN)
			{
				bool is_server = false;
				for (size_t j = 0; j < _server_fds.size(); ++j)
				{
					if (_fds[i].fd == _server_fds[j])
					{
						is_server = true;
						break;
					}
				}

				if (is_server)
				{
					acceptConnection(_fds[i].fd);
				}
				else
				{
					handleClientRead(_fds[i].fd);
				}
			}

			// 2. HANDLE WRITE (Send)
			if (_fds[i].revents & POLLOUT)
			{
				handleClientWrite(_fds[i].fd);
			}
		}
	}
}

/**
 * @brief Handle reading data from a client socket.
 * @param client_fd The file descriptor of the client socket.
 */
void Webserver::handleClientRead(int client_fd)
{
	char buffer[4096]; // Increased buffer size
	int bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

	if (bytes_read <= 0)
	{
		close(client_fd);
		_clients.erase(client_fd);
		// Remove from _fds loop... (implement the removal logic properly as before)
	}
	else
	{
		buffer[bytes_read] = '\0';

		// Feed the parser
		bool finished = _clients[client_fd].request.parse(std::string(buffer, bytes_read));

		if (finished)
		{
			std::cout << "Request Parsed!" << std::endl;
			// std::cout << "Method: " << _clients[client_fd].request.getMethod() << std::endl;
			// std::cout << "Path: " << _clients[client_fd].request.getPath() << std::endl;

			// // Prepare response (Temporary)
			// std::string body = "<html><body><h1>Parsed Successfully!</h1></body></html>";
			// std::stringstream ss;
			// ss << "HTTP/1.1 200 OK\r\n"
			//    << "Content-Type: text/html\r\n"
			//    << "Content-Length: " << body.size() << "\r\n"
			//    << "\r\n"
			//    << body;
			std::string response = HttpResponse::generateResponse(
				_clients[client_fd].request,
				*_configs_ptr,
				_clients[client_fd].listening_port);

			//_clients[client_fd].response_buffer = ss.str();
			_clients[client_fd].response_buffer = response;
			_clients[client_fd].is_ready_to_write = true;

			// Reset parser for next request on same connection (Keep-Alive)
			_clients[client_fd].request.reset();
		}
	}
}

// ONLY WRITES. Never calls recv().
/**
 * @brief Handle writing data to a client socket.
 * @param client_fd The file descriptor of the client socket.
 */
void Webserver::handleClientWrite(int client_fd)
{
	// Check if we actually have something to send
	if (_clients[client_fd].is_ready_to_write && !_clients[client_fd].response_buffer.empty())
	{

		std::string &response = _clients[client_fd].response_buffer;

		// Send as much as the OS allows
		int bytes_sent = send(client_fd, response.c_str(), response.size(), 0);

		if (bytes_sent < 0)
		{
			// Handle error
		}
		else
		{
			// Remove sent bytes from buffer
			if (bytes_sent > 0)
				response.erase(0, bytes_sent);

			// If buffer is empty, we are done sending
			if (response.empty())
			{
				_clients[client_fd].is_ready_to_write = false;
				std::cout << "Response fully sent to client " << client_fd << ". Closing connection." << std::endl;
				// If not keep-alive, close connection here
				// Still needs cleanup
			}
		}
	}
}

/**
 * @brief Accept a new client connection.
 * @param server_fd The file descriptor of the server socket.
 */
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
	new_client.listening_port = _server_fd_to_port[server_fd]; // You must map the server_fd to its port and store it in the client struct
	new_client.is_ready_to_write = false;
	_clients[client_fd] = new_client;

	std::cout << "New connection: " << client_fd << std::endl;
}

/**
 * @brief Handle reading data from a client socket.
 * @param client_fd The file descriptor of the client socket.
 */
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