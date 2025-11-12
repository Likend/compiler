// #pragma once

// #include <functional>
// #include <type_traits>

// #include "scope_hash_set.hpp"

// template <typename Key, typename Value, typename Hash = std::hash<Key>,
//           typename Pred = std::equal_to<Key>>
// class ScopeHashMap {
//    private:
//     struct Node {
//         Key key;
//         Value value;
//     };
//     struct MapHash {
//         Hash hash;
//         template <typename T>
//         size_t operator()(const T& node) {
//             return hash(node);
//         }
//         template <>
//         size_t operator()<Node>(const Node& node) {
//             return hash(node.key);
//         }
//     };
//     struct MapEqual {
//         Pred pred;
//         template <typename T, typename U>
//         bool operator()(const T& a, const U& b) {
//             if constexpr (std::is_same_v<T, Node> && std::is_same_v<U, Node>)
//             {
//                 return pred(a.key, b.key);
//             } else if constexpr (std::is_same_v<T, Node> &&
//                                  !std::is_same_v<U, Node>) {
//                 return pred(a.key, b);
//             } else if constexpr (!std::is_same_v<T, Node> &&
//                                  std::is_same_v<U, Node>) {
//                 return pred(a, b.key);
//             } else
//                 return pred(a, b);
//         }
//     };

//    public:
//     ScopeHashMap(size_t capacity = 256, Hash hash = Hash{}, Pred pred =
//     Pred{})
//         : set(capacity, MapHash{hash}, MapEqual{pred}) {}

//     size_t size() const noexcept { return set.size(); }
//     bool empty() const noexcept { return set.empty(); }

//     size_t insert(Key key, Value value) {
//         set.insert(Node{std::move(key), std::move(value)});
//     }

//     void rehash(size_t new_capacity) { set.rehash(new_capacity); }
//     void rehash() { rehash(set.bucket_count() * 2); }

//     using ScopeMarker = ScopeHashSet<Node, MapHash, MapEqual>::ScopeMarker;
//     ScopeMarker get_scope_marker() const { return set.get_scope_marker(); }

//     void pop_scope(ScopeMarker marker) { set.pop_scope(marker); }

//    private:
//     ScopeHashSet<Node, MapHash, MapEqual> set;
// };
