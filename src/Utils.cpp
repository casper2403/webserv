#include "Utils.hpp"
#include <sstream>
#include <sys/stat.h>
#include <fstream>
#include <stdexcept>

std::vector<std::string> split(const std::string &s, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delimiter)) {
        tokens.push_back(item);
    }
    return tokens;
}

std::string trim(const std::string &s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end = s.find_last_not_of(" \t\r\n");
    if (start == std::string::npos || end == std::string::npos) {
        return "";
    }
    return s.substr(start, end - start + 1);
}

bool fileExists(const std::string &path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

bool isDirectory(const std::string &path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

std::string readFile(const std::string &path) {
    std::ifstream file(path.c_str(), std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + path);
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

std::string joinPath(const std::string &root, const std::string &relative) {
    if (root.empty()) return relative;
    if (root[root.size() - 1] == '/') {
        return root + (relative.empty() ? "" : relative.substr(1));
    }
    if (!relative.empty() && relative[0] == '/') {
        return root + relative;
    }
    return root + "/" + relative;
}

std::string mimeType(const std::string &path) {
    size_t dot = path.find_last_of('.');
    std::string ext = dot == std::string::npos ? "" : path.substr(dot + 1);
    if (ext == "html" || ext == "htm") return "text/html";
    if (ext == "css") return "text/css";
    if (ext == "js") return "application/javascript";
    if (ext == "png") return "image/png";
    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "gif") return "image/gif";
    return "text/plain";
}
