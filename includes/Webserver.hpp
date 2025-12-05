#ifndef WEBSERVER_HPP
#define WEBSERVER_HPP

#include "Config.hpp"
#include <vector>
#include <poll.h>
#include <map>
#include <string>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <sys/wait.h>
#include "HttpRequest.hpp"

struct Client
{
    int fd;
    HttpRequest request;
    std::string response_buffer;
    bool is_ready_to_write;
    int listening_port;

    // CGI State
    bool is_cgi_active;
    int cgi_pid;
    int cgi_pipe_out; // Read from this
    std::string cgi_output_buffer;
	time_t cgi_start_time;

    Client() : fd(-1), is_ready_to_write(false), listening_port(0), 
               is_cgi_active(false), cgi_pid(-1), cgi_pipe_out(-1), cgi_start_time(0) {}
};

class Webserver
{
private:
    std::vector<struct pollfd> _fds;
    std::vector<int> _server_fds;
    std::map<int, Client> _clients;
    std::map<int, int> _cgi_fd_to_client_fd; // Maps CGI pipe FD -> Client FD

    void initSocket(int port);
    void acceptConnection(int server_fd);
    
    // Return true if connection is still active, false if closed/erased
    bool handleClientRead(int client_fd);
    void handleClientWrite(int client_fd);
    bool handleCgiRead(int cgi_fd);

    std::map<int, int> _server_fd_to_port;
    const std::vector<ServerConfig>* _configs_ptr;

public:
    Webserver();
    ~Webserver();

    void init(const std::vector<ServerConfig>& configs);
    void run();
};

#endif