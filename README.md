# Webserv

A non-blocking HTTP/1.1 web server written in C++98. Built as a 42 School project with support for multiple servers, static file serving, file uploads, and CGI execution.

## Features

- **Multi-Server Support** – Run multiple independent servers on different ports
- **Event-Driven I/O** – Non-blocking socket operations using `poll()`
- **HTTP/1.1 Parsing** – Handles headers, chunked transfer encoding, and request bodies
- **Static File Serving** – GET requests with proper Content-Type headers
- **File Uploads** – POST requests with multipart form data support
- **File Deletion** – DELETE method for removing files
- **Custom Configuration** – Nginx-like syntax with server and location blocks
- **Request Limits** – Configurable `client_max_body_size`
- **CGI Execution** – Run dynamic scripts with fork/execve
- **URL Redirects** – Support for 301/302 redirects
- **Directory Listing** – Autoindex for browsing directories
- **Custom Error Pages** – Map error codes to custom HTML pages

## Quick Start

### Build
```bash
make
```

### Run
```bash
./webserv [config_file]
```

Example:
```bash
./webserv conf_files/default.conf
```

### Clean
```bash
make clean      # Remove object files
make fclean     # Remove object files and binary
make re         # Rebuild from scratch
```

## Configuration

Configuration files use an Nginx-like syntax. Here's an example:

```nginx
server {
    listen 9090;
    host 127.0.0.1;
    server_name localhost;
    root ./www;
    client_max_body_size 10M;
    error_page 404 /404.html;

    location / {
        root ./www;
        index index.html;
        allow_methods GET;
    }

    location /uploads {
        root ./www;
        allow_methods GET POST DELETE;
        autoindex on;
    }
}
```

### Configuration Directives

| Directive | Example | Description |
|-----------|---------|-------------|
| `listen` | `listen 8080;` | Port to listen on |
| `host` | `host 127.0.0.1;` | Bind address |
| `server_name` | `server_name example.com;` | Server hostname |
| `root` | `root ./www;` | Document root directory |
| `client_max_body_size` | `client_max_body_size 10M;` | Max request body size |
| `error_page` | `error_page 404 /404.html;` | Custom error page mapping |
| `location` | `location /api { ... }` | Location block for path-specific config |
| `index` | `index index.html;` | Default file to serve for directories |
| `allow_methods` | `allow_methods GET POST;` | HTTP methods allowed for location |
| `autoindex` | `autoindex on;` | Enable directory listing |
| `return` | `return 301 /new-path;` | Redirect with status code |

## Architecture

### Core Components

```
includes/
├── Webserver.hpp     – Event loop and socket management
├── Config.hpp        – Configuration parser and structures
├── HttpRequest.hpp   – HTTP request parsing state machine
└── HttpResponse.hpp  – HTTP response generation

srcs/
├── main.cpp          – Entry point
├── Webserver.cpp     – Poll-based event handling
├── Config.cpp        – Configuration file parsing
├── HttpRequest.cpp   – Request parsing and chunked decoding
└── HttpResponse.cpp  – Response building for GET/POST/DELETE
```

### Request Flow

1. **Socket Binding** – Create listening sockets for each configured server
2. **Poll Loop** – Monitor all sockets for incoming connections and data
3. **Request Parsing** – Parse HTTP request headers and body using state machine
4. **Route Matching** – Match request path to configured locations
5. **Response Generation** – Generate appropriate HTTP response
6. **Non-Blocking I/O** – Send response asynchronously without blocking

## Compilation Details

- **Language Standard:** C++98
- **Compiler Flags:** `-Wall -Wextra -Werror`
- **Dependencies:** POSIX system calls only (no external libraries)

## Requirements

- C++98 compatible compiler
- POSIX-compliant system (Linux, macOS, etc.)
- Make


