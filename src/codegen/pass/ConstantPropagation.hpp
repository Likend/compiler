#pragma once

#include "codegen/MachineFunctionPass.hpp"

namespace codegen {
class ConstantPropagation final : public MachineFunctionPass {
   public:
    bool runOnMachineFunction(MachineFunction&) override;
};
}  // namespace codegen
