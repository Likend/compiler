#pragma once
#include <cassert>
#include <cstddef>
#include <functional>
#include <iterator>
#include <utility>
#include <vector>

template <typename Value, typename Hash = std::hash<Value>,
          typename Pred = std::equal_to<Value>, bool insert_back = false>
class ScopeHashSet {
   private:
    struct Node {
        Value  value;
        Node*  hash_next  = nullptr;
        Node** hash_prev  = nullptr;
        Node*  scope_next = nullptr;
    };

   public:
    ScopeHashSet(size_t capacity = 256, Hash hash = Hash{}, Pred pred = Pred{})
        : hash(hash), pred(pred) {
        assert(capacity >= 1);
        head  = new Node*;
        *head = nullptr;
        tail  = head;

        hash_buckets.resize(capacity, nullptr);
    }

    ~ScopeHashSet() { clean(); }

    ScopeHashSet(const ScopeHashSet&)            = delete;
    ScopeHashSet& operator=(const ScopeHashSet&) = delete;

    ScopeHashSet(ScopeHashSet&& other) {
        hash_buckets = std::move(other.hash_buckets);
        head         = other.head;
        tail         = other.tail;
        _size        = other._size;

        other.head  = nullptr;
        other.tail  = nullptr;
        other._size = 0;

        hash = std::move(other.hash);
        pred = std::move(other.pred);
    }

    ScopeHashSet& operator=(ScopeHashSet&& other) {
        if (this == &other) return *this;
        clean();
        hash_buckets = std::move(other.hash_buckets);
        head         = other.head;
        tail         = other.tail;
        _size        = other._size;

        other.head  = nullptr;
        other.tail  = nullptr;
        other._size = 0;

        hash = std::move(other.hash);
        pred = std::move(other.pred);
        return *this;
    }

    size_t size() const noexcept { return _size; }
    bool   empty() const noexcept { return _size == 0; }

    size_t bucket_count() const noexcept { return hash_buckets.size(); }

    // modify the return value may effect the hash.
    Value& insert(Value value) {
        size_t bucket_index = hash(value) % bucket_count();

        Node* new_node = new Node{std::move(value)};

        // 将新节点添加到作用域链表末尾
        *tail = new_node;                 // 将当前末尾指针指向新节点
        tail  = &(new_node->scope_next);  // 更新tail指向新节点的next字段地址

        insert_in_bucket(&hash_buckets[bucket_index], new_node);
        _size++;

        // 如果负载因子超标, 则 rehash
        if (load_factor() > max_load_factor) rehash();

        return new_node->value;
    }

    template <typename... Args>
    void emplace_back(Args&&... args) {
        insert(Value{std::forward<Args>(args)...});
    }

    void rehash(size_t new_capacity) {
        // Prevent rehashing to a smaller or equal size (no-op if not growing)
        if (new_capacity <= hash_buckets.size()) return;

        std::vector<Node*> new_buckets(new_capacity);

        for (Node* current = *head; current != nullptr;
             current       = current->scope_next) {
            size_t new_bucket_index = hash(current->value) % new_capacity;
            // current->hash_next = nullptr;
            // current->hash_prev = nullptr;
            insert_in_bucket(&new_buckets[new_bucket_index], current);
        }

        hash_buckets = std::move(new_buckets);
    }

    void rehash() { rehash(bucket_count() * 2); }

    struct ScopeMarker {
        Node** top;
    };

    ScopeMarker get_scope_marker() const { return {tail}; }

    void pop_scope(ScopeMarker marker) {
        // 获取当前作用域的开始位置
        Node** scope_start = marker.top;

        // 删除当前作用域的所有节点
        for (Node* current = *scope_start; current != nullptr;) {
            Node* next = current->scope_next;  // 保存下一个节点
            delete_in_bucket(current);         // 从哈希表中删除并释放内存
            current = next;                    // 移动到下一个节点
        }

        // 重设链表的结尾
        *scope_start = nullptr;
        tail         = scope_start;
    }

    struct iterator {
       private:
        const Node* node;
        friend class ScopeHashSet;

        iterator(Node* node) noexcept : node(node) {}

       public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = const Value;
        using pointer           = const Value*;
        using reference         = const Value&;

        reference operator*() const noexcept { return node->value; }
        pointer   operator->() const noexcept { return &node->value; }
        iterator& operator++() noexcept {
            node = node->scope_next;
            return *this;
        }
        iterator operator++(int) noexcept {
            auto temp = *this;
            ++(*this);
            return temp;
        }
        bool operator==(const iterator& other) const {
            return node == other.node;
        };
        bool operator!=(const iterator& other) const {
            return !this->operator==(other);
        };
    };

    iterator begin() const { return iterator{*head}; }
    iterator end() const { return iterator{*tail}; }

    template <typename SearchKey>
    iterator find(const SearchKey& key) const {
        size_t bucket_index = hash(key) % bucket_count();
        Node*  node         = search_in_bucket(bucket_index, key);
        return {node};
    }

    template <typename SearchKey>
    class EqualRange {
       private:
        Node*           node;
        const Pred&     pred;
        const SearchKey key;

        EqualRange(Node* node, const Pred& pred, const SearchKey& key)
            : node(node), pred(pred), key(key) {}
        friend class ScopeHashSet;

       public:
        struct iterator {
           private:
            Node*            node;
            const Pred&      pred;
            const SearchKey& key;
            friend class EqualRange;

            iterator(Node* node, const Pred& pred, const SearchKey& key)
                : node(node), pred(pred), key(key) {}

           public:
            using iterator_category = std::forward_iterator_tag;
            using difference_type   = std::ptrdiff_t;
            using value_type        = const Value;
            using pointer           = const Value*;
            using reference         = const Value&;

            reference operator*() const noexcept { return node->value; }
            pointer   operator->() const noexcept { return &node->value; }
            iterator& operator++() {
                do {
                    node = node->hash_next;
                } while (node != nullptr && !pred(node->value, key));
                return *this;
            }
            iterator operator++(int) noexcept {
                auto temp = *this;
                ++(*this);
                return temp;
            }
            bool operator==(const iterator& other) const noexcept {
                return node == other.node;
            }
            bool operator!=(const iterator& other) const noexcept {
                return !this->operator==(other);
            }
        };

        iterator begin() const noexcept { return {node, pred, key}; }
        iterator end() const noexcept { return {nullptr, pred, key}; }
    };

    template <typename SearchKey>
    EqualRange<SearchKey> equal_range(const SearchKey& key) const {
        size_t bucket_index = hash(key) % bucket_count();
        Node*  node         = search_in_bucket(bucket_index, key);
        return EqualRange<SearchKey>{node, pred, key};
    }

   private:
    Node** head;
    Node** tail;

    std::vector<Node*> hash_buckets;  // 哈希桶数组

    size_t _size = 0;

    Hash hash;
    Pred pred;

    template <typename SearchKey>
    Node* search_in_bucket(size_t bucket_index, const SearchKey& key) const {
        // 遍历哈希桶中的链表查找符号
        Node* node = hash_buckets[bucket_index];
        while (node != nullptr && !pred(node->value, key)) {
            node = node->hash_next;
        }
        return node;
    }

    // 把节点插入桶中, 保证节点在桶内的前后关系 (会处理 hash_prev, hash_next)
    // 不保证 scope 上的链条关系 (即不处理 head, tail, scope_next)
    // 不处理 rehash
    // 不处理 _size
    void insert_in_bucket(Node** pointer_to_current_node,
                          Node*  new_node) noexcept {
        assert(pointer_to_current_node);
        assert(new_node);

        if constexpr (!insert_back) {
            // 将新节点插入哈希桶头部
            Node* current_node  = *pointer_to_current_node;
            new_node->hash_next = current_node;  // 新节点指向当前头节点

            // 如果桶不为空，更新原头节点的prev_link_symbol
            if (current_node) {
                current_node->hash_prev = &(new_node->hash_next);
            }

            // 更新桶头指针指向新节点
            *pointer_to_current_node = new_node;
            new_node->hash_prev      = pointer_to_current_node;
        } else {
            // 将新节点插入哈希桶尾部
            while (*pointer_to_current_node != nullptr) {
                pointer_to_current_node =
                    &((*pointer_to_current_node)->hash_next);
            }

            *pointer_to_current_node = new_node;
            new_node->hash_next      = nullptr;
            new_node->hash_prev      = pointer_to_current_node;
        }
    }

    // 把节点从桶中删除, 保证节点在桶内的前后关系 (处理 hash_prev,
    // hash_next) 不保证 scope 上的链条关系 (即不处理 head, tail,
    // scope_next)
    void delete_in_bucket(Node* node) noexcept {
        assert(node);
        // 从哈希链表中删除节点
        Node* next_node = node->hash_next;
        if (next_node) {
            next_node->hash_prev = node->hash_prev;
        }
        *(node->hash_prev) = next_node;

        // 释放节点内存
        delete node;
        _size--;
    }

    constexpr static double max_load_factor = 0.75;
    double                  load_factor() const noexcept {
        return static_cast<double>(_size) / hash_buckets.size();
    }

    void clean() noexcept {
        if (head == nullptr) return;

        // 清理所有作用域，包括全局作用域
        for (Node* current = *head; current != nullptr;) {
            Node* next = current->scope_next;
            delete current;
            current = next;
        }

        delete head;
    }
};
