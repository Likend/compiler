#pragma once

#include "codegen/MachineFunction.hpp"
#include "codegen/MachineFunctionPass.hpp"

namespace codegen {
class RewriteLoadStorePass final : public MachineFunctionPass {
   public:
    bool runOnMachineFunction(MachineFunction&) override;
};
}  // namespace codegen
