#pragma once

#include "ir/Function.hpp"
#include "ir/Pass.hpp"

namespace codegen {
class MachineFunction;

class MachineFunctionPass : public ir::FunctionPass {
   public:
    virtual bool runOnMachineFunction(MachineFunction&) = 0;

   private:
    bool runOnFunction(ir::Function& f) override;
};
}  // namespace codegen
