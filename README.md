# Webserv – Project Plan of Attack

This document outlines a practical, ordered strategy for completing the Webserv project efficiently while avoiding the common pitfalls that lead to rewrites or failing mandatory requirements.

---

## 1. Essential Research (Do Before Coding)

### 1.1 HTTP 1.0/1.1 Basics
Understand:
- Request line format
- Headers (especially `Host`, `Content-Length`, `Connection`)
- Chunked transfer encoding (you must unchunk manually)
- Response structure
- Correct status code handling

### 1.2 Non-Blocking I/O + poll/select/epoll/kqueue
Learn:
- Non-blocking sockets (`fcntl(O_NONBLOCK)`)
- How a single poll loop drives all reads/writes
- Detecting disconnects without checking `errno` after I/O
- Why you must never read/write without readiness

### 1.3 Networking Basics
Know the behavior of:
`socket`, `bind`, `listen`, `accept`, `send`, `recv`, `fcntl`, `setsockopt`

### 1.4 CGI Fundamentals
Understand:
- Environment vars (PATH_INFO, QUERY_STRING, CONTENT_LENGTH…)
- Fork/execve only for CGI
- Feeding body to CGI stdin
- Reading CGI stdout and parsing headers
- Rules for EOF termination for CGI with/without Content-Length

---

## 2. Architecture Design (Before Implementation)

### 2.1 Core Modules
- Config parser  
- Poll/event loop manager  
- Server listener  
- Client connection handler  
- HTTP request parser (state machine)  
- Response builder  
- Route matcher  
- Static file/autoindex handling  
- CGI executor  

### 2.2 Necessary Data Structures
- `ServerConfig` objects with ports, routes, rules
- `RouteConfig` objects (methods, root, upload path, index, autoindex, redirect, CGI map)
- Client state (fd, buffers, parse state, parsed request)
- HTTP request object (method, path, headers, body, etc.)

---

## 3. Implementation Sequence (Strict Order)

### Step 1 — Configuration File
- Define grammar (simple NGINX-like sections)
- Parse and validate
- Produce `ServerConfig` objects

### Step 2 — Server Sockets + Poll Loop
- Create one socket per listen directive
- Set them non-blocking
- Add to poll()
- Implement event loop:
  - Accept connections
  - Read ready -> append to buffer
  - Write ready -> flush buffer

Test with simple echo server behavior.

### Step 3 — HTTP Request Parsing (State Machine)
Implement states:
1. Request line  
2. Headers  
3. Body decision  
4. Content-Length body  
5. Chunked body (unchunking required)  
6. Complete  

Validate:
- Oversized bodies
- Malformed headers  
- Client disconnects during parse

### Step 4 — Routing
Match:
- Server by IP:port
- Route by prefix  
Apply:
- Allowed methods  
- Redirects  
- Roots  
- Autoindex  
- Upload permissions  
- Default index

### Step 5 — Static File Handling
Implement:
- Path resolution  
- stat() checks  
- Serving files with correct MIME type  
- Autoindex  
- Default index file  
- Default error pages  

### Step 6 — HTTP Methods
Implement in this order:
1. **GET**  
2. **DELETE**  
3. **POST**  
   - Upload handling
   - Normal POST (static or CGI depending on route)

### Step 7 — CGI Execution
Implement:
- Extension → interpreter mapping  
- Fork  
- Set environment  
- Pipe CGI stdout/stderr  
- Feed body to CGI stdin  
- Parse returned headers  

Handle:
- Chunked requests → unchunk before giving to CGI  
- Output without Content-Length → EOF termination  

### Step 8 — Proper HTTP Response Generation
Ensure:
- Accurate status codes  
- Content-Length rules  
- Keep-alive handling  
- Custom error pages  
- Redirects (301/302)  

### Step 9 — Stress Testing
Use:
- `ab`
- `wrk`
- custom Python load scripts

Check:
- No memory leaks  
- No hangs  
- Clean disconnect handling  
- Stable under many concurrent connections  

---

## 4. Critical Things Not to Forget
- Never read/write without poll readiness  
- Only one poll()/equivalent for all sockets  
- No checking `errno` after read/write to adjust behavior  
- Chunked decoding is mandatory  
- Uploads restricted to configured routes  
- Autoindex must be toggleable  
- Default error pages required  
- Multiple listening ports required  
- Request must never hang  
- Proper keep-alive behavior  
- CGI must receive all required environment variables  

---

## 5. Short Roadmap

1. Research HTTP + non-blocking I/O  
2. Build config parser  
3. Implement non-blocking poll loop  
4. Implement client class + buffers  
5. Build full HTTP parser (state machine)  
6. Implement routing  
7. Static file serving  
8. Methods: GET → DELETE → POST  
9. CGI execution  
10. Error pages, redirects, autoindex  
11. Keep-alive + protocol polish  
12. Stress testing  
13. Cleanup & documentation  
