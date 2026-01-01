#pragma once

namespace ir {
class User;
class Value;
class Use {
   private:
    Use(User* parent, Value* val = nullptr) : val(val), parent(parent) {}

   public:
    Use(const Use& U) = default;

    operator Value*() const { return val; }

    Value* get() const { return val; }
    User*  getUser() const { return parent; }
    void   set(Value* val);

    Use& operator=(const Use& rhs) {
        if (this != &rhs) set(rhs.val);
        return *this;
    }

    Use* getNext() const { return next; }

   private:
    friend class User;
    friend class Value;
    Value* val    = nullptr;  // 被使用的 Value
    User*  parent = nullptr;  // 包含这个 Use 的 User

    Use*  next = nullptr;
    Use** prev = nullptr;

    void addToList(Use** list);
    void removeFromList();
};
}  // namespace ir
