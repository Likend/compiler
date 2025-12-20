#pragma once

#include <initializer_list>

#include "codegen/LinerScanRegisterAlloc.hpp"
#include "codegen/MachineFunctionPass.hpp"
#include "codegen/Register.hpp"
#include "ir/Module.hpp"
#include "util/assert.hpp"

namespace codegen {
class MachineFunction;

class ReplaceRegisterPass final : public MachineFunctionPass {
   public:
    ReplaceRegisterPass(std::initializer_list<Register> spareRegister)
        : regPool(spareRegister) {}

   private:
    void doInitialization(ir::Module&) override {
        allocationAnalysis = &getAnalysis<RegisterAllocationAnalysis>();
    }
    void doFinalization(ir::Module&) override {
        removeAnalysis<RegisterAllocationAnalysis>();
    }
    void runOnMachineFunction(MachineFunction&) override;

    Register allocReg(size_t i) {
        ASSERT(i < regPool.size());
        return regPool[i];
    }

    RegisterAllocationAnalysis* allocationAnalysis;

    std::vector<Register> regPool;
};
}  // namespace codegen
