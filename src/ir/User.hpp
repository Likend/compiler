#pragma once

#include <vector>

#include "ir/Use.hpp"
#include "ir/Value.hpp"
#include "util/assert.hpp"
#include "util/iterator_range.hpp"

namespace ir {
class User : public Value {
   protected:
    std::vector<Use> operandList;

   public:
    User(Type* ty, size_t numOperands)
        : Value(ty), operandList(numOperands, Use(this)) {}

    void setOperand(size_t i, Value* v) {
        ASSERT(i < getNumOperands());
        operandList[i].set(v);
    }
    Value* getOperand(size_t i) const {
        ASSERT(i < getNumOperands());
        return operandList[i].get();
    }
    const Use& getOperandUse(size_t i) const {
        ASSERT(i < getNumOperands());
        return operandList[i];
    }
    Use& getOperandUse(size_t i) {
        ASSERT(i < getNumOperands());
        return operandList[i];
    }
    size_t getNumOperands() const { return operandList.size(); }

    using op_iterator        = decltype(operandList)::iterator;
    using const_op_interator = decltype(operandList)::const_iterator;
    using op_range           = iterator_range<op_iterator>;
    using const_op_range     = iterator_range<const_op_interator>;

    // clang-format off
    op_iterator        op_begin()       { return operandList.begin(); }
    const_op_interator op_begin() const { return operandList.cbegin(); }
    op_iterator        op_end()         { return operandList.end(); }
    const_op_interator op_end()   const { return operandList.cend(); }
    op_range           operands()       { return op_range{op_begin(), op_end()}; }
    const_op_range     operands() const { return const_op_range{op_begin(), op_end()}; }
    // clang-format on

    void dropAllReferences() {
        for (Use& use : operands()) use.set(nullptr);
    }

    void growUses(unsigned newNumUses) { operandList.reserve(newNumUses); }
};
}  // namespace ir
