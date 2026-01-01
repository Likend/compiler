#pragma once

#include <memory>
#include <type_traits>
#include <utility>

#include "util/assert.hpp"

template <typename NodeT>
struct IntrusiveNodeTraits {
    using intrusive_node_type = typename NodeT::intrusive_node_type;

    static intrusive_node_type* next(intrusive_node_type* curr) {
        return NodeT::next(curr);
    }

    static intrusive_node_type* prev(intrusive_node_type* curr) {
        return NodeT::prev(curr);
    }

    static void link_between(intrusive_node_type* curr,
                             intrusive_node_type* prev,
                             intrusive_node_type* next) {
        return NodeT::link_between(curr, prev, next);
    }

    static void unlink(intrusive_node_type* curr) {
        return NodeT::unlink(curr);
    }

    static bool is_independent(const intrusive_node_type* curr) {
        return NodeT::is_independent(curr);
    }

    static void reset_from(intrusive_node_type* curr,
                           intrusive_node_type* other) {
        return NodeT::reset_from(curr, other);
    }

    template <typename... Args>
    static intrusive_node_type* create_sentinel(Args&&... args) {
        return NodeT::create_sentinel(std::forward<Args>(args)...);
    }
};

class IntrusiveNode {
    template <typename T>
    friend struct IntrusiveNodeTraits;

    IntrusiveNode* prev_;
    IntrusiveNode* next_;

    using intrusive_node_type = IntrusiveNode;

   public:
    IntrusiveNode() : prev_(this), next_(this) {}

   protected:
    static IntrusiveNode* next(IntrusiveNode* curr) { return curr->next_; }
    static IntrusiveNode* prev(IntrusiveNode* curr) { return curr->prev_; }

    static void link_between(IntrusiveNode* curr, IntrusiveNode* prev,
                             IntrusiveNode* next) {
        ASSERT(is_independent(curr));
        ASSERT(prev->next_ == next);
        ASSERT(next->prev_ == prev);

        curr->prev_ = prev;
        curr->next_ = next;
        prev->next_ = curr;
        next->prev_ = curr;
    }

    static void unlink(IntrusiveNode* curr) {
        curr->prev_->next_ = curr->next_;
        curr->next_->prev_ = curr->prev_;
        curr->prev_ = curr->next_ = curr;
    }

    static bool is_independent(const IntrusiveNode* curr) {
        return curr->next_ == curr;
    }

    static void reset_from(IntrusiveNode*, IntrusiveNode*) {}

    static IntrusiveNode* create_sentinel() { return new IntrusiveNode{}; }
};

template <typename Parent>
class IntrusiveNodeWithParent : IntrusiveNode {
    template <typename T>
    friend struct IntrusiveNodeTraits;

    Parent* parent_ = nullptr;

    using intrusive_node_type = IntrusiveNodeWithParent<Parent>;

   public:
    IntrusiveNodeWithParent() = default;

    Parent*       parent() { return parent_; }
    const Parent* parent() const { return parent_; }

   protected:
    IntrusiveNodeWithParent(Parent* parent) : parent_(parent) {}

    static IntrusiveNodeWithParent* next(IntrusiveNodeWithParent* curr) {
        return static_cast<IntrusiveNodeWithParent*>(IntrusiveNode::next(curr));
    }
    static IntrusiveNodeWithParent* prev(IntrusiveNodeWithParent* curr) {
        return static_cast<IntrusiveNodeWithParent*>(IntrusiveNode::prev(curr));
    }

    static void link_between(IntrusiveNodeWithParent* curr,
                             IntrusiveNodeWithParent* prev,
                             IntrusiveNodeWithParent* next) {
        ASSERT(prev->parent_ == next->parent_);
        IntrusiveNode::link_between(curr, prev, next);
        curr->parent_ = prev->parent_;
    }

    static void unlink(IntrusiveNodeWithParent* curr) {
        IntrusiveNode::unlink(curr);
        curr->parent_ = nullptr;
    }

    static void reset_from(IntrusiveNodeWithParent* curr,
                           IntrusiveNodeWithParent* other) {
        curr->parent_ = other->parent_;
    }

    static IntrusiveNodeWithParent* create_sentinel(Parent& parent) {
        return new IntrusiveNodeWithParent{&parent};
    }
};

namespace intrusive_list_detail {

template <typename T, typename Derive>
class IntrusiveListIterBase {
   protected:
    using traits         = IntrusiveNodeTraits<T>;
    using intrusive_node = typename traits::intrusive_node_type;

   private:
    Derive& self() { return static_cast<Derive&>(*this); }

    static_assert(std::is_base_of_v<intrusive_node, std::remove_const_t<T>>,
                  "T must derive from IntrusiveNode");

   protected:
    template <typename AnotherT, typename AnotherDerive>
    friend class IntrusiveListIterBase;

    template <typename TT>
    friend class IntrusiveList;

    intrusive_node* curr = nullptr;

   public:
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type        = T;
    using pointer           = T*;
    using reference         = T&;
    using difference_type   = std::ptrdiff_t;

    IntrusiveListIterBase() = default;
    IntrusiveListIterBase(intrusive_node* curr) : curr(curr) {}
    IntrusiveListIterBase(T& t) : curr(static_cast<intrusive_node*>(&t)) {}

    template <typename AnotherT, typename AnotherDerive,
              std::enable_if_t<std::is_convertible_v<AnotherT*, T*>, int> = 0>
    IntrusiveListIterBase(
        const IntrusiveListIterBase<AnotherT, AnotherDerive>& other)
        : curr(other.curr) {}

    reference operator*() const { return *static_cast<T*>(curr); }
    pointer   operator->() const { return static_cast<T*>(curr); }

    Derive& operator++() {
        curr = self().next();
        return self();
    }
    Derive& operator--() {
        curr = self().prev();
        return self();
    }

    Derive operator++(int) {
        Derive ret = self();
        ++self();
        return ret;
    }
    Derive operator--(int) {
        Derive ret = self();
        --self();
        return ret;
    }

    bool operator==(const Derive& other) const { return curr == other.curr; }
    bool operator!=(const Derive& other) const { return curr != other.curr; }

    void unlink() {
        static_assert(!std::is_const_v<std::remove_reference_t<T>>);
        traits::unlink(curr);
    }

    void insert_into(Derive& pos) {
        static_assert(!std::is_const_v<std::remove_reference_t<T>>);
        traits::link_between(curr, pos.prev(), pos.curr);
    }

    bool is_independent() const { return traits::is_independent(curr); }
};
}  // namespace intrusive_list_detail

template <typename T>
class IntrusiveIterator : public intrusive_list_detail::IntrusiveListIterBase<
                              T, IntrusiveIterator<T>> {
    using Base =
        intrusive_list_detail::IntrusiveListIterBase<T, IntrusiveIterator<T>>;
    using typename Base::intrusive_node;
    using typename Base::traits;

    template <typename TT, typename Derive>
    friend class intrusive_list_detail::IntrusiveListIterBase;

    intrusive_node* next() const { return traits::next(this->curr); }
    intrusive_node* prev() const { return traits::prev(this->curr); }

   public:
    using Base::Base;
};

template <typename T>
IntrusiveIterator(T&) -> IntrusiveIterator<T>;

template <typename T>
IntrusiveIterator(T*) -> IntrusiveIterator<T>;

template <typename T>
class IntrusiveReverseIterator
    : public intrusive_list_detail::IntrusiveListIterBase<
          T, IntrusiveReverseIterator<T>> {
    using Base = intrusive_list_detail::IntrusiveListIterBase<
        T, IntrusiveReverseIterator<T>>;
    using typename Base::intrusive_node;
    using typename Base::traits;

    template <typename TT, typename Derive>
    friend class intrusive_list_detail::IntrusiveListIterBase;

    intrusive_node* next() const { return traits::prev(this->curr); }
    intrusive_node* prev() const { return traits::next(this->curr); }

   public:
    using Base::Base;
};

template <typename T>
IntrusiveReverseIterator(T&) -> IntrusiveReverseIterator<T>;

template <typename T>
IntrusiveReverseIterator(T*) -> IntrusiveReverseIterator<T>;

template <typename T>
class IntrusiveList {
   private:
    using traits         = IntrusiveNodeTraits<T>;
    using intrusive_node = typename traits::intrusive_node_type;

    static_assert(std::is_base_of_v<intrusive_node, T>,
                  "T must derive from IntrusiveNode");

    std::unique_ptr<intrusive_node> sentinel;

   public:
    using value_type             = T;
    using reference              = value_type&;
    using const_reference        = const value_type&;
    using pointer                = T*;
    using const_pointer          = const T*;
    using iterator               = IntrusiveIterator<T>;
    using reverse_iterator       = IntrusiveReverseIterator<T>;
    using const_iterator         = IntrusiveIterator<const T>;
    using const_reverse_iterator = IntrusiveReverseIterator<const T>;

    template <typename... Args>
    IntrusiveList(Args&&... args)
        : sentinel(traits::create_sentinel(std::forward<Args>(args)...)) {}

    ~IntrusiveList() { clear(); }

    IntrusiveList(IntrusiveList&& other) noexcept
        : sentinel(std::move(other.sentinel)) {
        other.sentinel = std::make_unique<intrusive_node>();
        traits::reset_from(other.sentinel.get(), sentinel.get());
    }
    IntrusiveList& operator=(IntrusiveList&& other) noexcept {
        if (this != &other) {
            clear();
            sentinel       = std::move(other.sentinel);
            other.sentinel = std::make_unique<intrusive_node>();
            traits::reset_from(other.sentinel.get(), sentinel.get());
        }
        return *this;
    }

    IntrusiveList(const IntrusiveList&)            = delete;
    IntrusiveList& operator=(const IntrusiveList&) = delete;

    // clang-format off
    reference       front()       { ASSERT(!empty()); return *begin(); }
    const_reference front() const { ASSERT(!empty()); return *begin(); }
    reference       back()        { ASSERT(!empty()); return *rbegin(); }
    const_reference back()  const { ASSERT(!empty()); return *rbegin(); }

    iterator               begin()        { return {traits::next(sentinel.get())}; }
    const_iterator         begin()  const { return {traits::next(sentinel.get())}; }
    iterator               end()          { return {sentinel.get()}; }
    const_iterator         end()    const { return {sentinel.get()}; }
    const_iterator         cbegin() const { return {traits::next(sentinel.get())}; }
    const_iterator         cend()   const { return {sentinel.get()}; }
    reverse_iterator       rbegin()       { return {traits::prev(sentinel.get())}; }
    const_reverse_iterator rbegin() const { return {traits::prev(sentinel.get())}; }
    reverse_iterator       rend()         { return {sentinel.get()}; }
    const_reverse_iterator rend()   const { return {sentinel.get()}; }

    bool   empty() const { return traits::is_independent(sentinel.get()); }
    size_t size()  const { return std::distance(begin(), end()); }
    // clang-format on

    void push_back(T* node) {
        link_before(sentinel.get(), static_cast<intrusive_node*>(node));
    }
    void push_front(T* node) {
        link_before(traits::next(sentinel.get()),
                    static_cast<intrusive_node*>(node));
    }

    iterator insert(iterator pos, T* node) {
        link_before(static_cast<intrusive_node*>(const_cast<T*>(&*pos)),
                    static_cast<intrusive_node*>(node));
        return iterator{*node};
    }

    void pop_back() { erase(rbegin()); }
    void pop_front() { erase(begin()); }

    template <class... Args>
    iterator emplace(iterator pos, Args&&... args) {
        T* node = new T{std::forward<Args>(args)...};
        return insert(pos, node);
    }

    template <class... Args>
    reference emplace_back(Args&&... args) {
        push_back(new T{std::forward<Args>(args)...});
        return back();
    }

    template <class... Args>
    reference emplace_front(Args&&... args) {
        push_front(new T{std::forward<Args>(args)...});
        return front();
    }

    iterator erase(iterator it) {
        ASSERT(!empty());
        iterator cur = it++;
        cur.unlink();
        delete &*cur;
        return it;
    }

    void clear() {
        iterator it = begin();
        while (it != end()) {
            it = erase(it);
        }
    }

    void splice(iterator pos, IntrusiveList& other) {
        return splice(pos, other, other.begin(), other.end());
    }
    void splice(iterator pos, IntrusiveList& other, iterator first) {
        return splice(pos, other, first, other.end());
    }
    void splice(iterator pos, [[maybe_unused]] IntrusiveList& other,
                iterator first, iterator last) {
        for (auto it = first; it != last;) {
            iterator cur = it++;
            cur.unlink();
            insert(pos, &*cur);
        }
    }

   private:
    void link_before(intrusive_node* pos, intrusive_node* node) {
        traits::link_between(node, traits::prev(pos), pos);
    }
};
