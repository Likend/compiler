#pragma once

#include "ir/Instructions.hpp"
#include "ir/LLVMContext.hpp"
#include "ir/Value.hpp"
#include "ir/ValueSymbolTable.hpp"
#include "util/IntrusiveList.hpp"

namespace ir {
class Function;

class BasicBlock final : public Value,
                         public ValueNode<BasicBlock, Function>,
                         public IntrusiveList<Instruction> {
    friend class Function;
    friend class Instruction;

   public:
    explicit BasicBlock(LLVMContext& c, std::string name = "")
        : Value(Type::getLabelTy(c)), IntrusiveList<Instruction>(*this) {
        setName(std::move(name));
    }

    static BasicBlock* Create(LLVMContext& c, std::string name,
                              Function* parent = nullptr);

    BasicBlock(const BasicBlock&)            = delete;
    BasicBlock& operator=(const BasicBlock&) = delete;

    const Module& getModule() const;
    Module&       getModule();
};
}  // namespace ir
