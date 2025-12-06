#pragma once

#include <iterator>
#include <utility>
template <typename Derived, typename BaseIter, typename TransformToT>
class iterator_transform {
   protected:
    BaseIter current_it;

   public:
    using base_iterator_reference =
        typename std::iterator_traits<BaseIter>::reference;
    using iterator_category =
        typename std::iterator_traits<BaseIter>::iterator_category;
    using difference_type =
        typename std::iterator_traits<BaseIter>::difference_type;
    using reference  = TransformToT&;
    using pointer    = TransformToT*;
    using value_type = std::remove_reference_t<reference>;

    iterator_transform() = default;
    iterator_transform(BaseIter base_it) : current_it(std::move(base_it)) {}

    Derived& operator++() {
        ++current_it;
        return static_cast<Derived&>(*this);
    }

    Derived operator++(int) {
        Derived temp = static_cast<Derived&>(*this);
        ++current_it;
        return temp;
    }

    bool operator==(const Derived& other) const {
        return current_it == other.current_it;
    }

    bool operator!=(const Derived& other) const {
        return current_it != other.current_it;
    }

    reference operator*() const {
        return static_cast<const Derived*>(this)->transform(*current_it);
    }

    reference operator*() {
        return static_cast<Derived*>(this)->transform(*current_it);
    }

    pointer operator->() { return &operator*(); }
    pointer operator->() const { return &operator*(); }
};
