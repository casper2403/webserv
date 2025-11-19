#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <vector>

std::vector<std::string> split(const std::string &s, char delimiter);
std::string trim(const std::string &s);
bool fileExists(const std::string &path);
bool isDirectory(const std::string &path);
std::string readFile(const std::string &path);
std::string joinPath(const std::string &root, const std::string &relative);
std::string mimeType(const std::string &path);

#endif
