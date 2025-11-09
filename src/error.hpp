#pragma once

#include <cstddef>
#include <vector>

struct ErrorInfo {
    char type;
    size_t line;
    size_t col;

    inline bool operator==(const ErrorInfo& other) const {
        return type == other.type && line == other.line && col == other.col;
    }

    inline bool operator<(const ErrorInfo& other) const {
        if (line < other.line)
            return true;
        else if (line == other.line) {
            if (col < other.col)
                return true;
            else if ((col == other.col) && (type < other.type))
                return true;
        }
        return false;
    }
};

namespace std {
template <>
struct hash<ErrorInfo> {
    size_t operator()(const ErrorInfo& info) const {
        return std::hash<char>{}(info.type) ^
               (std::hash<size_t>{}(info.line) < 1) ^
               (std::hash<size_t>{}(info.col) < 2);
    }
};
}  // namespace std

extern std::vector<ErrorInfo> error_infos;
