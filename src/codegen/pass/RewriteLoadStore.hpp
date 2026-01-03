#pragma once

#include "codegen/MachineFunction.hpp"
#include "codegen/MachineFunctionPass.hpp"
#include "codegen/MachineInstr.hpp"

namespace codegen {
class RewriteLoadStorePass final : public MachineFunctionPass {
   public:
    bool runOnMachineFunction(MachineFunction&) override;

   private:
    bool propagateAddi(MachineInstr&);
    bool propagateFrame(MachineInstr&);
};
}  // namespace codegen
