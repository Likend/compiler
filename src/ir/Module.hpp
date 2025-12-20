#pragma once
#include <memory>
#include <string_view>

#include "ir/Constants.hpp"
#include "ir/ValueSymbolTable.hpp"
#include "util/iterator_range.hpp"
#include "util/iterator_transform.hpp"

namespace ir {
class GlobalVariable;
class Function;
class LLVMContext;
class Module {
   private:
    LLVMContext&                                 context;
    std::vector<std::unique_ptr<GlobalVariable>> globalList;
    std::vector<std::unique_ptr<Function>>       functionList;
    std::string                                  moduleID;
    ValueSymbolTable                             symbolTable;
    // std::string sourceFileName;
    friend class Constant;
    friend class Function;
    friend class GlobalVariable;

   public:
    explicit Module(std::string moduleID, LLVMContext& c);
    std::string_view getName() const { return moduleID; }

    Function*       getFunction(std::string_view name) const;
    GlobalVariable* getGlobalVariable(std::string_view name) const;

    const ValueSymbolTable& getValueSymbolTable() const { return symbolTable; }
    ValueSymbolTable&       getValueSymbolTable() { return symbolTable; }

    struct global_iterator
        : public iterator_transform<
              global_iterator, decltype(globalList)::iterator, GlobalVariable> {
        using iterator_transform::iterator_transform;
        GlobalVariable& transform(std::unique_ptr<GlobalVariable>& i) const {
            return *i;
        }
    };
    struct const_global_iterator
        : public iterator_transform<const_global_iterator,
                                    decltype(globalList)::const_iterator,
                                    const GlobalVariable> {
        using iterator_transform::iterator_transform;
        const GlobalVariable& transform(
            const std::unique_ptr<GlobalVariable>& i) const {
            return *i;
        }
    };

    using global_range       = iterator_range<global_iterator>;
    using const_global_range = iterator_range<const_global_iterator>;

    global_iterator       global_begin() { return {globalList.begin()}; }
    const_global_iterator global_begin() const { return {globalList.begin()}; }
    global_iterator       global_end() { return {globalList.end()}; }
    const_global_iterator global_end() const { return {globalList.end()}; }
    global_range          globals() { return {global_begin(), global_end()}; }
    const_global_range    globals() const {
        return {global_begin(), global_end()};
    }

    struct iterator
        : public iterator_transform<iterator, decltype(functionList)::iterator,
                                    Function> {
        using iterator_transform::iterator_transform;
        Function& transform(std::unique_ptr<Function>& i) const { return *i; }
    };
    struct const_iterator
        : public iterator_transform<const_iterator,
                                    decltype(functionList)::const_iterator,
                                    const Function> {
        using iterator_transform::iterator_transform;
        const Function& transform(const std::unique_ptr<Function>& i) const {
            return *i;
        }
    };

    iterator       begin() { return {functionList.begin()}; }
    const_iterator begin() const { return functionList.begin(); }
    iterator       end() { return {functionList.end()}; }
    const_iterator end() const { return {functionList.end()}; }
};
}  // namespace ir
