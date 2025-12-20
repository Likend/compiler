#pragma once

#include <unordered_map>

#include "codegen/MachineBasicBlock.hpp"
#include "codegen/MachineFunctionPass.hpp"
#include "codegen/Register.hpp"
#include "ir/BasicBlock.hpp"
#include "ir/DataLayout.hpp"
#include "ir/Instructions.hpp"
#include "ir/Value.hpp"

namespace codegen {
class MachineFunction;

class IRTranslator final : public MachineFunctionPass {
   private:
    std::unordered_map<const ir::Value*, Register> vmap;

    MachineFunction*   currentFunction;
    MachineBasicBlock* currentBB;

    std::unordered_map<const ir::BasicBlock*, MachineBasicBlock*> bbMap;

    ir::DataLayout dl;

    Register getVReg(const ir::Value*);
    Register CreateVReg(const ir::Value*);
    Register getOrCreateVReg(const ir::Value*);
    void AssignVReg(const ir::Value*, Register);

    // void assignPhysicReg(const ir::Value& val, Register);

    void runOnMachineFunction(MachineFunction& mf) override;

    void translateBasicBlock(const ir::BasicBlock*);
    void translateInstruction(const ir::Instruction*);

    Register prepareReg(const ir::Value*);
};
}  // namespace codegen
