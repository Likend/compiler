#pragma once

#include "ir/BasicBlock.hpp"
#include "ir/GlobalValue.hpp"
#include "ir/LLVMContext.hpp"
#include "ir/Type.hpp"
#include "ir/Value.hpp"
#include "ir/ValueSymbolTable.hpp"
#include "util/IntrusiveList.hpp"

namespace ir {
class Argument final : public Value, public IntrusiveNodeWithParent<Function> {
    size_t argNo;

    friend class Function;

   public:
    explicit Argument(Type* ty, unsigned argNo = 0, std::string name = "")
        : Value(ty), argNo(argNo) {
        setName(std::move(name));
    };

    unsigned getArgNo() const { return argNo; }
};

class Function : public GlobalObject,
                 public IntrusiveNodeWithParent<Module>,
                 public IntrusiveList<BasicBlock> {
    friend BasicBlock;
    friend Value;

    ValueSymbolTable symbolTable;

   private:
    Function(FunctionType* ty, LinkageTypes linkage, std::string name = "",
             unsigned addrSpace = 0);

   public:
    IntrusiveList<Argument> args;

    ~Function() override;
    Function(const Function&)       = delete;
    void operator=(const Function&) = delete;

    static Function* Create(FunctionType* ty, LinkageTypes linkage,
                            unsigned addrSpace, std::string name,
                            Module& module);
    static Function* Create(FunctionType* ty, LinkageTypes linkage,
                            std::string name, Module& module) {
        return Create(ty, linkage, 0, std::move(name), module);
    }

    FunctionType* getFunctionType() const {
        return static_cast<FunctionType*>(getValueType());
    }

    Type* getReturnType() const { return getFunctionType()->getReturnType(); }

    LLVMContext& getContext() const { return getType()->getContext(); }

    const BasicBlock& getEntryBlock() const { return front(); }
    BasicBlock&       getEntryBlock() { return front(); }

    const ValueSymbolTable& getValueSymbolTable() const { return symbolTable; }
    ValueSymbolTable&       getValueSymbolTable() { return symbolTable; }

    // Functions are definitions if they have a body.
    bool isDeclaration() const override { return empty(); }

   private:
    void buildArguments();
};
}  // namespace ir
