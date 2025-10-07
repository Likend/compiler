#pragma once

#include <cstring>
#include <functional>
#include <string_view>
#include <unordered_set>
#include <vector>

class StringPool {
   public:
    StringPool() { memory.reserve(1024); }

    StringPool(const StringPool&) = delete;

    std::string_view intern(std::string_view str);

    inline size_t size() { return memory.size(); }

   private:
    struct RelativeStringView {
        size_t offset;
        size_t size;

        struct Hash {
            // using is_transparent = void;

            Hash(const std::vector<char>& memory) : memory(std::ref(memory)) {}

            inline size_t operator()(RelativeStringView rel_sv) const {
                auto str = StringPool::get_string_view(memory.get(), rel_sv);
                return std::hash<std::string_view>{}(str);
            }

            // inline size_t operator()(std::string_view sv) const {
            //     return std::hash<std::string_view>{}(sv);
            // }

           private:
            std::reference_wrapper<const std::vector<char>> memory;
        };

        struct Eqaul {
            // using is_transparent = void;

            Eqaul(const std::vector<char>& memory) : memory(std::ref(memory)) {}

            inline size_t operator()(RelativeStringView a,
                                     RelativeStringView b) const {
                return std::equal_to<std::string_view>{}(
                    StringPool::get_string_view(memory.get(), a),
                    StringPool::get_string_view(memory.get(), b));
            }

            // inline size_t operator()(std::string_view a,
            //                          RelativeStringView b) const {
            //     return std::equal_to<std::string_view>{}(
            //         a, StringPool::get_string_view(memory.get(), b));
            // }

            // inline size_t operator()(RelativeStringView a,
            //                          std::string_view b) const {
            //     return std::equal_to<std::string_view>{}(
            //         StringPool::get_string_view(memory.get(), a), b);
            // }

           private:
            std::reference_wrapper<const std::vector<char>> memory;
        };
    };

    static inline std::string_view get_string_view(
        const std::vector<char>& memory, const RelativeStringView& rel_sv) {
        const char* data = memory.data() + rel_sv.offset;
        return {data, rel_sv.size};
    }

    std::vector<char> memory;
    std::unordered_set<RelativeStringView, RelativeStringView::Hash,
                       RelativeStringView::Eqaul>
        index{11, RelativeStringView::Hash{memory},
              RelativeStringView::Eqaul{memory}};
};
