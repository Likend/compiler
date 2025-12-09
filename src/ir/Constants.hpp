#pragma once

#include <cstdint>

#include "ir/LLVMContext.hpp"
#include "ir/Type.hpp"
#include "ir/User.hpp"
#include "ir/Value.hpp"
namespace ir {
class Constant : public User {
   protected:
    Constant(Type* ty, size_t numOperands) : User(ty, numOperands) {}

   public:
    void operator=(const Constant&) = delete;
    Constant(const Constant&)       = delete;
};

class ConstantData : public Constant {
    friend class Constant;

   protected:
    explicit ConstantData(Type* ty) : Constant(ty, 0) {}

   public:
    ConstantData(const ConstantData&) = delete;
};

class ConstantInt final : public ConstantData {
    friend class Constant;

    int32_t val;

    ConstantInt(IntegerType* ty, int32_t val) : ConstantData(ty), val(val) {}

   public:
    ConstantInt(const ConstantInt&) = delete;

    static ConstantInt* get(IntegerType* ty, int32_t v);

    int32_t  getValue() const { return val; }
    unsigned getBitWidth() const { return getIntegerType()->getBitWidth(); }

    IntegerType* getIntegerType() const {
        return static_cast<IntegerType*>(Value::getType());
    }
};

class ConstantAggregate : public Constant {
   protected:
    ConstantAggregate(Type* ty, const std::vector<Constant*>& val)
        : Constant(ty, val.size()) {
        for (size_t i = 0; i < val.size(); i++) {
            setOperand(i, val[i]);
        }
    }
};

class ConstantArray final : public ConstantAggregate {
    ConstantArray(ArrayType* ty, const std::vector<Constant*>& val)
        : ConstantAggregate(ty, val) {}

   public:
    static ConstantArray* get(ArrayType* ty, const std::vector<Constant*>& val);

    ArrayType* getType() const {
        return static_cast<ArrayType*>(Value::getType());
    }
};

class ConstantString final : public ConstantData {
    std::string str;
    ConstantString(LLVMContext& c, std::string str);

   public:
    static ConstantString* getString(LLVMContext& c, std::string str);

    std::string_view getAsString() const { return str; }
};

class ConstantAggregateZero final : public ConstantData {
    ConstantAggregateZero(Type* ty) : ConstantData(ty) {}

   public:
    static ConstantAggregateZero* get(Type* ty);
};
}  // namespace ir
