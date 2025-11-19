#ifndef HTTP_REQUEST_HPP
#define HTTP_REQUEST_HPP

#include <string>
#include <map>

class HttpRequest {
public:
    HttpRequest();
    bool parse(const std::string &raw);

    const std::string &method() const;
    const std::string &path() const;
    const std::map<std::string, std::string> &headers() const;

private:
    std::string _method;
    std::string _path;
    std::map<std::string, std::string> _headers;
    void clear();
};

#endif
