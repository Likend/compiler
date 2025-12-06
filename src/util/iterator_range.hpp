#pragma once

#include <utility>

template <typename IteratorT>
class iterator_range {
    IteratorT begin_it, end_it;

   public:
    iterator_range(IteratorT begin_it, IteratorT end_it)
        : begin_it(std::move(begin_it)), end_it(std::move(end_it)) {}

    IteratorT begin() const { return begin_it; }
    IteratorT end() const { return end_it; }
    bool      empty() const { return begin_it == end_it; }
};

template <typename T>
iterator_range<T> make_range(T x, T y) {
    return iterator_range<T>(std::move(x), std::move(y));
}

template <typename T>
iterator_range<T> make_range(std::pair<T, T> p) {
    return iterator_range<T>(std::move(p.first), std::move(p.second));
}
