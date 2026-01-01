#pragma once

#include "codegen/MachineFunctionPass.hpp"

namespace codegen {
class MipsMachineMulDivOpt final : public MachineFunctionPass {
   public:
    bool runOnMachineFunction(MachineFunction&) override;
};
}  // namespace codegen
