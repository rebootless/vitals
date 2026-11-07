#pragma once

#include <string>

inline std::string str_trim(const std::string& s) {
    const size_t start = s.find_first_not_of(" \t\r\n");
    const size_t end   = s.find_last_not_of(" \t\r\n");
    return start == std::string::npos ? "" : s.substr(start, end - start + 1);
}
