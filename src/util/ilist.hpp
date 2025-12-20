#pragma once

#include <cstddef>
#include <iterator>

#include "util/assert.hpp"

template <typename BaseT>
class ilist;

template <typename BaseT>
class ilist_hook {
    BaseT* next_;
    BaseT* prev_;
    friend ilist<BaseT>;

   public:
    ilist_hook() { prev_ = next_ = static_cast<BaseT*>(this); }

    BaseT* next() const { return next_; }
    BaseT* prev() const { return prev_; }
    void   take() {
        next_->prev_ = prev_;
        prev_->next_ = next_;
        prev_ = next_ = static_cast<BaseT*>(this);
    }
    void insertAfter(BaseT* t) {
        ASSERT(next_ == prev_ == static_cast<BaseT*>(this));
        t->next_->prev_ = this;
        this->next_     = t->next_;
        t->next_        = this;
        this->prev_     = t;
    }
};

template <typename BaseT>
class ilist_iterator {
    BaseT* b = nullptr;

   public:
    using iterator_category = std::bidirectional_iterator_tag;
    using difference_type   = std::ptrdiff_t;
    using value_type        = BaseT;
    using pointer           = BaseT*;
    using reference         = BaseT&;

    ilist_iterator() = default;
    ilist_iterator(BaseT* b) : b(b) {}

    bool operator==(ilist_iterator<BaseT> other) const { return b == other.b; }
    bool operator!=(ilist_iterator<BaseT> other) const { return b != other.b; }

    ilist_iterator<BaseT>& operator++() {
        b = b->next();
        return *this;
    }

    ilist_iterator<BaseT> operator++(int) {
        BaseT* pack = b;
        b           = b->next();
        return {pack};
    }

    ilist_iterator<BaseT>& operator--() {
        b = b->prev();
        return *this;
    }

    ilist_iterator<BaseT> operator--(int) {
        BaseT* pack = b;
        b           = b->next();
        return {pack};
    }

    reference operator*() { return *b; }
    pointer   operator->() { return b; }
};

template <typename BaseT>
class ilist {
    BaseT* sentinel;
};
