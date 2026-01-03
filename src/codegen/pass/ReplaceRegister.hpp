#pragma once

#include <initializer_list>

#include "codegen/MachineFunctionPass.hpp"
#include "codegen/pass/LinerScanRegisterAlloc.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterInfo.hpp"
#include "ir/Module.hpp"
#include "util/assert.hpp"

namespace codegen {
class MachineFunction;

class ReplaceRegisterPass final : public MachineFunctionPass {
   public:
    ReplaceRegisterPass(std::initializer_list<Register> spareRegister)
        : regPool(spareRegister) {}

   private:
    bool doInitialization(ir::Module&) override {
        allocationAnalysis = &getAnalysis<RegisterAllocationAnalysis>();
        return false;
    }
    bool doFinalization(ir::Module&) override {
        removeAnalysis<RegisterAllocationAnalysis>();
        return false;
    }
    bool runOnMachineFunction(MachineFunction&) override;

    Register allocReg(size_t i) {
        ASSERT(i < regPool.size());
        return regPool[i];
    }

    RegisterAllocationAnalysis* allocationAnalysis;

    std::vector<Register> regPool;

    void ReplacePhysic(MachineFunction& mf, RegisterInfo& info,
                       Register targetReg);
    void ReplaceStack(MachineFunction& mf, RegisterInfo& info);
};
}  // namespace codegen
