#include "string_pool.hpp"

//// C++20 version
// std::string_view StringPool::intern(std::string_view str) {
//     if (auto find = index.find(str); find != index.end()) {
//         return get_string_view(memory, *find);
//     }
//     size_t prev_size = memory.size();
//     size_t new_size = prev_size + str.size();
//     if (new_size > memory.capacity()) {
//         memory.reserve(memory.capacity() + 1024);
//     }
//     memory.resize(new_size);
//     char* dst = memory.data() + prev_size;
//     std::memcpy(dst, str.data(), str.size());

//     index.insert(RelativeStringView{prev_size, str.size()});
//     return {dst, str.size()};
// }

std::string_view StringPool::intern(std::string_view str) {
    size_t prev_size = memory.size();
    size_t new_size = prev_size + str.size();
    if (new_size > memory.capacity()) {
        memory.reserve(memory.capacity() + 1024);
    }
    memory.resize(new_size);
    char* dst = memory.data() + prev_size;
    std::memcpy(dst, str.data(), str.size());

    RelativeStringView rsv{prev_size, str.size()};
    if (auto find = index.find(rsv); find != index.end()) {
        memory.resize(prev_size);
        return get_string_view(memory, *find);
    }

    index.insert(rsv);
    return {dst, rsv.size};
}
