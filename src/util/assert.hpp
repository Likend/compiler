#include <sstream>
#include <stdexcept>

#define UNREACHABLE()                       \
    do {                                    \
        std::stringstream ss;               \
        ss << "Unreachable! ";              \
        ANNOTATE_LOCATION(ss);              \
        throw std::runtime_error(ss.str()); \
    } while (0)

#define ANNOTATE_LOCATION(os) \
    os << '(' << __FUNCTION__ << " @ " << __FILE__ << ':' << __LINE__ << ')'
