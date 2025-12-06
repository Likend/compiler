#pragma once

#include <memory>
#include <ostream>
#include <vector>

#include "ir/GlobalValue.hpp"
#include "ir/LLVMContext.hpp"
#include "ir/Type.hpp"
#include "ir/Value.hpp"
#include "ir/ValueSymbolTable.hpp"
#include "util/iterator_range.hpp"
#include "util/iterator_transform.hpp"

namespace ir {
class BasicBlock;
class Argument;

class Function : public GlobalObject {
    friend BasicBlock;
    friend Value;
    std::vector<std::unique_ptr<BasicBlock>> basicBlocks;
    std::vector<std::unique_ptr<Argument>>   arguments;
    ValueSymbolTable                         symbolTable;

   private:
    Function(FunctionType* ty, LinkageTypes linkage, unsigned addrSpace,
             std::string name, Module& module);

   public:
    ~Function() override;
    Function(const Function&)       = delete;
    void operator=(const Function&) = delete;

    static Function* Create(FunctionType* ty, LinkageTypes linkage,
                            unsigned addrSpace, std::string name,
                            Module& module) {
        return new Function(ty, linkage, addrSpace, std::move(name), module);
    }
    static Function* Create(FunctionType* ty, LinkageTypes linkage,
                            std::string name, Module& module) {
        return Create(ty, linkage, 0, std::move(name), module);
    }

    FunctionType* getFunctionType() const {
        return static_cast<FunctionType*>(getValueType());
    }

    Type* getReturnType() const { return getFunctionType()->getReturnType(); }

    LLVMContext& getContext() const { return getType()->getContext(); }

    const BasicBlock& getEntryBlock() const { return *basicBlocks.front(); }
    BasicBlock&       getEntryBlock() { return *basicBlocks.front(); }

    const ValueSymbolTable& getValueSymbolTable() const { return symbolTable; }
    ValueSymbolTable&       getValueSymbolTable() { return symbolTable; }

    // Functions are definitions if they have a body.
    bool isDeclaration() const override { return basicBlocks.empty(); }

    struct arg_iterator
        : public iterator_transform<arg_iterator, decltype(arguments)::iterator,
                                    Argument> {
        using iterator_transform::iterator_transform;
        Argument& transform(std::unique_ptr<Argument>& a) const { return *a; }
    };
    struct const_arg_iterator
        : public iterator_transform<const_arg_iterator,
                                    decltype(arguments)::const_iterator,
                                    const Argument> {
        using iterator_transform::iterator_transform;
        Argument& transform(const std::unique_ptr<Argument>& a) const {
            return *a;
        }
    };
    using arg_range       = iterator_range<arg_iterator>;
    using const_arg_range = iterator_range<const_arg_iterator>;

    arg_iterator       arg_begin() { return {arguments.begin()}; }
    const_arg_iterator arg_begin() const { return {arguments.begin()}; }
    arg_iterator       arg_end() { return {arguments.end()}; }
    const_arg_iterator arg_end() const { return {arguments.end()}; }
    arg_range          args() { return {arg_begin(), arg_end()}; }
    const_arg_range    args() const { return {arg_begin(), arg_end()}; }
    size_t             arg_size() const { return arguments.size(); }

    struct iterator
        : public iterator_transform<iterator, decltype(basicBlocks)::iterator,
                                    BasicBlock> {
        using iterator_transform::iterator_transform;
        BasicBlock& transform(std::unique_ptr<BasicBlock>& i) const {
            return *i;
        }
    };
    struct const_iterator
        : public iterator_transform<const_iterator,
                                    decltype(basicBlocks)::const_iterator,
                                    const BasicBlock> {
        using iterator_transform::iterator_transform;
        const BasicBlock& transform(
            const std::unique_ptr<BasicBlock>& i) const {
            return *i;
        }
    };

    iterator          begin() { return {basicBlocks.begin()}; }
    const_iterator    begin() const { return {basicBlocks.begin()}; }
    iterator          end() { return {basicBlocks.end()}; }
    const_iterator    end() const { return {basicBlocks.end()}; }
    size_t            size() const { return basicBlocks.size(); }
    bool              empty() const { return basicBlocks.empty(); }
    BasicBlock&       front() { return *begin(); }
    const BasicBlock& front() const { return *begin(); }
    BasicBlock&       back() { return *end(); }
    const BasicBlock& back() const { return *end(); }

   private:
    void buildArguments();
};

class Argument final : public Value {
    Function* parent;
    size_t    argNo;

    friend class Function;

   public:
    explicit Argument(Type* ty, std::string name = "", Function* f = nullptr,
                      unsigned argNo = 0)
        : Value(ty), parent(f), argNo(argNo) {
        setName(std::move(name));
    };

    Function* getParent() const { return parent; }
    unsigned  getArgNo() const { return argNo; }
};

std::ostream& operator<<(std::ostream&, const Function*);
}  // namespace ir
