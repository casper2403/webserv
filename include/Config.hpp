#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <vector>
#include <stdexcept>

struct RouteConfig {
    std::string root;
    bool autoindex;
    std::string index;

    RouteConfig() : root("./www"), autoindex(false), index("index.html") {}
};

struct ServerConfig {
    int port;
    RouteConfig route;
    ServerConfig() : port(8080), route() {}
};

class ConfigParser {
public:
    explicit ConfigParser(const std::string &path);
    const std::vector<ServerConfig> &servers() const;

private:
    std::vector<ServerConfig> _servers;
    void parse(const std::string &path);
};

#endif
