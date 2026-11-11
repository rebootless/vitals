#pragma once

#include <cctype>
#include <string>
#include <string_view>

inline std::string str_trim(const std::string& s) {
    const size_t start = s.find_first_not_of(" \t\r\n");
    const size_t end   = s.find_last_not_of(" \t\r\n");
    return start == std::string::npos ? "" : s.substr(start, end - start + 1);
}

inline bool str_starts(const std::string& s, std::string_view prefix) {
    return s.compare(0, prefix.size(), prefix) == 0;
}

inline std::string str_lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
