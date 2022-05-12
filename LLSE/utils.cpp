#include "utils.hpp"

#include <sys/stat.h>
#include <iostream>
#include <iomanip>

bool is_file(const std::string& path) {
    struct stat sb;

    if (stat(path.c_str(), &sb) == 0 && S_ISREG(sb.st_mode)) {
        return true;
    }
    return false;
}

bool is_directory(const std::string& path) {
    struct stat sb;

    if (stat(path.c_str(), &sb) == 0 && S_ISDIR(sb.st_mode)) {
        return true;
    }
    return false;
}

bool exists(const std::string& path) {
    struct stat sb;

    if (stat(path.c_str(), &sb) == 0) {
        return true;
    }
    return false;
}

bool create_directory(const std::string& path, mode_t mode) {
    if (mkdir(path.data(), mode) != 0) {
        return false;
    }
    return true;
}

std::string hex_string(const std::string& in) {
    std::ostringstream out;
    for (unsigned char c : in) {
        out << std::hex << std::setw(2) << std::setfill('0') << (uint) c;
    }
    return out.str();
}

std::ostream& print_hex(std::ostream& out, const std::string &s) {
    for (unsigned char c : s) {
        out << std::hex << std::setw(2) << std::setfill('0') << (uint) c;
    }

    return out;
}

std::string hex_string(const uint64_t &a) {
    std::ostringstream out;
    out << std::hex << std::setw(16) << std::setfill('0') << (uint64_t) a;
    return out.str();
}

std::string hex_string(const uint32_t &a) {
    std::ostringstream out;
    out << std::hex << std::setw(8) << std::setfill('0') << (uint32_t) a;
    return out.str();
}

void append_keyword_map(std::ostream& out, const std::string &kw, uint32_t index) {
    out << kw << "       " << std::hex << index << "\n";
}