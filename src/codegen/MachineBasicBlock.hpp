#pragma once

#include <list>
#include <memory>

#include "codegen/MachineInstr.hpp"
#include "codegen/MachineOperand.hpp"

namespace codegen {
class MachineFunction;

class MachineBasicBlock {
   public:
    // const ir::BasicBlock* bb;
    MachineFunction& parent;

    /// Keep track of the predecessor / successor basic blocks.
    std::vector<MachineBasicBlock*> predecessors;
    std::vector<MachineBasicBlock*> successors;

    std::list<std::unique_ptr<MachineInstr>> instrs;

    std::string name;

   public:
    MachineBasicBlock(MachineFunction& parent) : parent(parent) {}

    MachineFunction&       getParent() { return parent; }
    const MachineFunction& getParent() const { return parent; }

    struct iterator
        : public iterator_transform<iterator, decltype(instrs)::iterator,
                                    MachineInstr> {
        using iterator_transform::iterator_transform;
        MachineInstr& transform(std::unique_ptr<MachineInstr>& i) const {
            return *i;
        }

       private:
        friend class MachineBasicBlock;
        auto& origin_it() { return current_it; }
    };
    struct const_iterator
        : public iterator_transform<const_iterator,
                                    decltype(instrs)::const_iterator,
                                    const MachineInstr> {
        using iterator_transform::iterator_transform;
        const MachineInstr& transform(
            const std::unique_ptr<MachineInstr>& i) const {
            return *i;
        }

       private:
        friend class MachineBasicBlock;
        auto& origin_it() { return current_it; }
    };

    iterator       begin() { return {instrs.begin()}; }
    const_iterator begin() const { return {instrs.begin()}; }
    iterator       end() { return {instrs.end()}; }
    const_iterator end() const { return {instrs.end()}; }

    MachineInstr&       front() { return *instrs.front().get(); }
    const MachineInstr& front() const { return *instrs.front().get(); }
    MachineInstr&       back() { return *instrs.back().get(); }
    const MachineInstr& back() const { return *instrs.back().get(); }

    bool empty() const { return instrs.empty(); }

    MachineInstr& emplace(const MachineInstrDesc&                      desc,
                          std::initializer_list<MachineOperandContent> ops) {
        return *instrs
                    .emplace_back(
                        std::make_unique<MachineInstr>(*this, desc, ops))
                    .get();
    }

    template <typename... OperandArgT>
    MachineInstr& emplace(const MachineInstrDesc& desc, OperandArgT... arg) {
        return emplace(desc, {MachineOperandContent{arg}...});
    }

    iterator insert(iterator position, const MachineInstrDesc& desc,
                    std::initializer_list<MachineOperandContent> ops) {
        position.origin_it() =
            instrs.emplace(position.origin_it(),
                           std::make_unique<MachineInstr>(*this, desc, ops));
        iterator ret = position;
        ++position.origin_it();
        return ret;
    }

    template <typename... OperandArgT>
    iterator insert(iterator position, const MachineInstrDesc& desc,
                    OperandArgT... arg) {
        return insert(position, desc, {MachineOperandContent{arg}...});
    }

    iterator erase(iterator it);
};
}  // namespace codegen
