#pragma once

#include "ir/Function.hpp"
#include "ir/Pass.hpp"

namespace codegen {
class MachineFunction;

class MachineFunctionPass : public ir::FunctionPass {
   public:
    virtual void runOnMachineFunction(MachineFunction&) = 0;

   private:
    void runOnFunction(ir::Function& f) override;
};
}  // namespace codegen
