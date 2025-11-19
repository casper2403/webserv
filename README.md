# Webserv – Minimal HTTP/1.1 Server

This repository contains a lean reference implementation of the 42 **webserv** project. The goal is to provide a clear, well-commented baseline that demonstrates the core requirements: configuration parsing, non-blocking sockets with `poll`, and basic HTTP request/response handling.

## Features
- C++98 codebase with a small, dependency-free build.
- Non-blocking TCP listeners using `poll` to multiplex clients.
- Simple NGINX-like configuration file that supports multiple servers.
- Static file serving with rudimentary MIME type detection.
- Minimal error handling for missing files and forbidden directory access.

## Building
```bash
make
```

## Running
Provide a configuration file (defaults to `config/webserv.conf`):
```bash
./webserv [config_file]
```

Example configuration:
```nginx
server {
    listen 8080;
    root ./www;
    index index.html;
    autoindex off;
}
```

## Notes
- This implementation focuses on clarity over completeness. It does **not** yet implement chunked decoding, uploads, DELETE, or CGI execution, but the structure leaves room to extend those features.
- Responses are sent with `Connection: close`, so each request ends the connection.
