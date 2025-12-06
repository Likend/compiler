#pragma once

#include <memory>
#include <vector>

#include "ir/LLVMContext.hpp"
#include "ir/Value.hpp"
#include "util/iterator_transform.hpp"
namespace ir {
class Function;
class Instruction;

class BasicBlock final : public Value {
    friend class Function;
    friend class Instruction;
    Function*                                 parent;
    std::vector<std::unique_ptr<Instruction>> instList;

   private:
    void setParent(Function* parent) { this->parent = parent; }

    explicit BasicBlock(LLVMContext& c, std::string name = "",
                        Function* parent = nullptr);

   public:
    ~BasicBlock() override;
    BasicBlock(const BasicBlock&)            = delete;
    BasicBlock& operator=(const BasicBlock&) = delete;

    static BasicBlock* Create(LLVMContext& c, std::string name,
                              Function* parent = nullptr) {
        return new BasicBlock{c, std::move(name), parent};
    }

    const Function* getParent() const { return parent; }
    Function*       getParent() { return parent; }

    const Module* getModule() const;
    Module*       getModule();

    struct iterator
        : public iterator_transform<iterator, decltype(instList)::iterator,
                                    Instruction> {
        using iterator_transform::iterator_transform;
        Instruction& transform(std::unique_ptr<Instruction>& i) const {
            return *i;
        }
    };
    struct const_iterator
        : public iterator_transform<const_iterator,
                                    decltype(instList)::const_iterator,
                                    const Instruction> {
        using iterator_transform::iterator_transform;
        const Instruction& transform(
            const std::unique_ptr<Instruction>& i) const {
            return *i;
        }
    };

    iterator           begin() { return {instList.begin()}; }
    const_iterator     begin() const { return {instList.begin()}; }
    iterator           end() { return {instList.end()}; }
    const_iterator     end() const { return {instList.end()}; }
    size_t             size() const { return instList.size(); }
    bool               empty() const { return instList.empty(); }
    Instruction&       front() { return *instList.front(); }
    const Instruction& front() const { return *instList.front(); }
    Instruction&       back() { return *instList.back(); }
    const Instruction& back() const { return *instList.back(); }

   private:
    const auto& getInstList() const { return instList; }
    auto&       getInstList() { return instList; }
};
}  // namespace ir
