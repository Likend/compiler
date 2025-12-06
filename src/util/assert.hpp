#pragma once

#include <cstddef>
#include <sstream>
#include <stdexcept>
#include <string_view>

[[noreturn]] static inline void assert_throw(std::string_view err_msg,
                                             std::string_view func_name,
                                             std::string_view file_name,
                                             size_t           file_line) {
    std::stringstream ss;
    ss << err_msg << '(' << func_name << " @ " << file_name << ':' << file_line
       << ')';
    throw std::runtime_error(ss.str());
}

#define ANNOTATE_LOCATION __FUNCTION__, __FILE__, __LINE__

#define UNREACHABLE() assert_throw("Unreachable! ", ANNOTATE_LOCATION)

#define ASSERT_WITH(x, msg)                   \
    if (x) {                                  \
    } else {                                  \
        assert_throw(msg, ANNOTATE_LOCATION); \
    }

#define ASSERT(x) ASSERT_WITH(x, "Assert failed! ")
