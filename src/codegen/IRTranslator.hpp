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
    void     AssignVReg(const ir::Value*, Register);

    void runOnMachineFunction(MachineFunction& mf) override;

    void translateBasicBlock(const ir::BasicBlock*);
    void translateInstruction(const ir::Instruction*);

    void translateBinaryOperator(const ir::BinaryOperator*);
    void translateICmpInst(const ir::ICmpInst*);
    void translateAllocaInst(const ir::AllocaInst*);
    void translateLoadInst(const ir::LoadInst*);
    void translateStoreInst(const ir::StoreInst*);
    void translateGetElementPtrInst(const ir::GetElementPtrInst*);
    void translateReturnInst(const ir::ReturnInst*);
    void translateBranchInst(const ir::BranchInst*);
    void translateCastInst(const ir::CastInst*);
    void translateCallInst(const ir::CallInst*);

    Register prepareReg(const ir::Value*);
};
}  // namespace codegen
