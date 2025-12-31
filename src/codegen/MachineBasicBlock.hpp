#pragma once

#include "codegen/MachineInstr.hpp"
#include "codegen/MachineOperand.hpp"
#include "util/IntrusiveList.hpp"

namespace codegen {
class MachineFunction;

class MachineBasicBlock : public IntrusiveNodeWithParent<MachineFunction>,
                          public IntrusiveList<MachineInstr> {
   public:
    // const ir::BasicBlock* bb;

    /// Keep track of the predecessor / successor basic blocks.
    std::vector<MachineBasicBlock*> predecessors;
    std::vector<MachineBasicBlock*> successors;

    std::string name;

   public:
    MachineBasicBlock(std::string name = "")
        : IntrusiveList<MachineInstr>(*this), name(std::move(name)) {}
};
}  // namespace codegen
