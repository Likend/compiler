#pragma once

#include <string>

#include "ir/LLVMContext.hpp"

namespace ir {
class Use;
class User;
class Type;

class Value {
   protected:
    std::string name;

   private:
    Type* ty;
    Use*  useList = nullptr;

   protected:
    Value(Type* ty) : ty(ty) {}

   public:
    virtual ~Value() = default;

    std::string_view getName() const { return name; }
    void             setName(std::string name);

    Type*        getType() const { return ty; }
    LLVMContext& getContext() const;

    void addUse(Use& user);
    bool use_empty() const { return useList == nullptr; }
    // TODO: use iterator and user iterator

    // // 打印所有使用该 Value 的 User
    // void printUses() const;  // 在 User 定义后实现
};
}  // namespace ir
