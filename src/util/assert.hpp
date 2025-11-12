#include <sstream>
#include <stdexcept>

#define ANNOTATE_LOCATION(os) \
    os << '(' << __FUNCTION__ << " @ " << __FILE__ << ':' << __LINE__ << ')'

#define UNREACHABLE()                       \
    do {                                    \
        std::stringstream ss;               \
        ss << "Unreachable! ";              \
        ANNOTATE_LOCATION(ss);              \
        throw std::runtime_error(ss.str()); \
    } while (0)

#define ASSERT(x)                           \
    if (!(x)) {                             \
        std::stringstream ss;               \
        ss << "Assert failed! ";            \
        ANNOTATE_LOCATION(ss);              \
        throw std::runtime_error(ss.str()); \
    }
