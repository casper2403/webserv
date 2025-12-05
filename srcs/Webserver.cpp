#include "../includes/Webserver.hpp"
#include "../includes/Config.hpp"
#include "../includes/HttpResponse.hpp"
#include <algorithm> // For std::find

Webserver::Webserver() {}
Webserver::~Webserver() {}

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

		if (!port_exists)
		{
			initSocket(port);
			listening_ports.push_back(port);
			std::cout << "Server initialized on port " << port << std::endl;
		}
	}
}

void Webserver::initSocket(int port)
{
	int server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd < 0)
	{
		perror("socket failed");
		exit(EXIT_FAILURE);
	}

	int opt = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
	{
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}

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

	struct pollfd pfd;
	pfd.fd = server_fd;
	pfd.events = POLLIN;
	pfd.revents = 0;
	_fds.push_back(pfd);
	_server_fds.push_back(server_fd);
	_server_fd_to_port[server_fd] = port;
}

void Webserver::run()
{
	std::cout << "Waiting for connections..." << std::endl;

	while (true)
	{
		int ret = poll(&_fds[0], _fds.size(), -1);
		if (ret < 0)
		{
			perror("poll");
			break;
		}
		time_t now = time(NULL);
		for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it)
		{
			if (it->second.is_cgi_active && (now - it->second.cgi_start_time) > 3)
			{
				std::cout << "CGI Timeout for Client " << it->first << std::endl;

				// Kill the hanging process
				kill(it->second.cgi_pid, SIGKILL);
				waitpid(it->second.cgi_pid, NULL, 0);

				// Clean up pipes from poll
				int cgi_fd = it->second.cgi_pipe_out;
				close(cgi_fd);
				_cgi_fd_to_client_fd.erase(cgi_fd);

				// Remove cgi_fd from _fds vector
				for (std::vector<struct pollfd>::iterator p_it = _fds.begin(); p_it != _fds.end(); ++p_it)
				{
					if (p_it->fd == cgi_fd)
					{
						_fds.erase(p_it);
						break;
					}
				}

				// Send 504 Gateway Timeout
				it->second.is_cgi_active = false;
				it->second.response_buffer = "HTTP/1.1 504 Gateway Timeout\r\nContent-Length: 0\r\n\r\n";
				it->second.is_ready_to_write = true;
			}
		}
		// Iterate safely: if we erase an element, we must NOT increment 'i'
		for (size_t i = 0; i < _fds.size(); /* i incremented manually */)
		{
			bool fd_removed = false;

			// READ EVENTS (Include POLLHUP for hang-ups)
			if (_fds[i].revents & (POLLIN | POLLHUP))
			{
				int fd = _fds[i].fd;
				bool is_server = false;
				for (size_t j = 0; j < _server_fds.size(); ++j)
				{
					if (fd == _server_fds[j])
					{
						is_server = true;
						break;
					}
				}

				if (is_server)
				{
					acceptConnection(fd);
				}
				else if (_cgi_fd_to_client_fd.count(fd))
				{
					// handleCgiRead returns false if it closed the FD
					if (!handleCgiRead(fd))
						fd_removed = true;
				}
				else
				{
					// handleClientRead returns false if it closed the FD
					if (!handleClientRead(fd))
						fd_removed = true;
				}
			}

			// WRITE EVENTS (Only if FD wasn't just removed)
			if (!fd_removed && (_fds[i].revents & POLLOUT))
			{
				int fd = _fds[i].fd;
				if (_clients.count(fd))
				{
					handleClientWrite(fd);
				}
			}

			// Only increment if we didn't remove the current element
			// (When removed, the next element shifts into current index 'i')
			if (!fd_removed)
			{
				++i;
			}
		}
	}
}

bool Webserver::handleClientRead(int client_fd)
{
	char buffer[4096];
	int bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

	if (bytes_read <= 0)
	{
		close(client_fd);
		_clients.erase(client_fd);

		// Remove from _fds vector
		for (std::vector<struct pollfd>::iterator it = _fds.begin(); it != _fds.end(); ++it)
		{
			if (it->fd == client_fd)
			{
				_fds.erase(it);
				break;
			}
		}
		return false; // FD was removed
	}
	else
	{
		buffer[bytes_read] = '\0';
		bool finished = _clients[client_fd].request.parse(std::string(buffer, bytes_read));

		if (finished)
		{
			std::cout << "Request Parsed! Processing..." << std::endl;

			// Pass Client Ref to Logic
			HttpResponse::processRequest(_clients[client_fd], *_configs_ptr);

			// If logic started a CGI script, add its pipe to poll
			if (_clients[client_fd].is_cgi_active)
			{
				int cgi_fd = _clients[client_fd].cgi_pipe_out;
				struct pollfd pfd;
				pfd.fd = cgi_fd;
				pfd.events = POLLIN; // POLLHUP is implicitly handled by poll
				pfd.revents = 0;
				_fds.push_back(pfd);
				_cgi_fd_to_client_fd[cgi_fd] = client_fd;
				std::cout << "CGI started. Monitoring pipe " << cgi_fd << std::endl;
			}

			_clients[client_fd].request.reset();
		}
		return true; // FD kept
	}
}

bool Webserver::handleCgiRead(int cgi_fd)
{
	char buffer[4096];
	int bytes_read = read(cgi_fd, buffer, sizeof(buffer) - 1);

	// Safety check if client disconnected while CGI was running
	if (_cgi_fd_to_client_fd.find(cgi_fd) == _cgi_fd_to_client_fd.end())
	{
		close(cgi_fd);
		for (std::vector<struct pollfd>::iterator it = _fds.begin(); it != _fds.end(); ++it)
		{
			if (it->fd == cgi_fd)
			{
				_fds.erase(it);
				break;
			}
		}
		return false;
	}

	int client_fd = _cgi_fd_to_client_fd[cgi_fd];

	if (bytes_read > 0)
	{
		buffer[bytes_read] = '\0';
		_clients[client_fd].cgi_output_buffer += buffer;
		return true; // FD kept
	}
	else
	{
		// CGI Finished (EOF or Error)
		close(cgi_fd);
		for (std::vector<struct pollfd>::iterator it = _fds.begin(); it != _fds.end(); ++it)
		{
			if (it->fd == cgi_fd)
			{
				_fds.erase(it);
				break;
			}
		}
		_cgi_fd_to_client_fd.erase(cgi_fd);

		waitpid(_clients[client_fd].cgi_pid, NULL, 0); // Reap zombie

		std::string response = HttpResponse::buildCgiResponse(_clients[client_fd].cgi_output_buffer);
		_clients[client_fd].response_buffer = response;
		_clients[client_fd].is_ready_to_write = true;
		_clients[client_fd].is_cgi_active = false;
		std::cout << "CGI Finished. Response built." << std::endl;

		return false; // FD removed
	}
}

void Webserver::handleClientWrite(int client_fd)
{
	if (_clients[client_fd].is_ready_to_write && !_clients[client_fd].response_buffer.empty())
	{
		std::string &response = _clients[client_fd].response_buffer;
		int bytes_sent = send(client_fd, response.c_str(), response.size(), 0);

		if (bytes_sent > 0)
			response.erase(0, bytes_sent);

		if (response.empty())
		{
			_clients[client_fd].is_ready_to_write = false;
			std::cout << "Response fully sent." << std::endl;
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
	if (fcntl(client_fd, F_SETFL, O_NONBLOCK) < 0)
	{
		perror("fcntl client");
		close(client_fd);
		return;
	}

	struct pollfd pfd;
	pfd.fd = client_fd;
	pfd.events = POLLIN | POLLOUT;
	pfd.revents = 0;
	_fds.push_back(pfd);

	Client new_client;
	new_client.fd = client_fd;
	new_client.listening_port = _server_fd_to_port[server_fd];
	_clients[client_fd] = new_client;

	std::cout << "New connection: " << client_fd << std::endl;
}