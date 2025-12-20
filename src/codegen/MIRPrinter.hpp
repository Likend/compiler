#pragma once

#include <ostream>

#include "codegen/Register.hpp"
#include "ir/Module.hpp"
#include "ir/Pass.hpp"

namespace codegen {
class MachineFunction;
class MachineBasicBlock;
class MachineInstr;
class MachineOperand;

class MIRPrinterPass final : public ir::ImmutablePass {
   public:
    MIRPrinterPass(std::ostream& os) : os(os) {}

    void doInitialization(ir::Module&) override;

   private:
    std::ostream& os;

    void printFunction(MachineFunction&);
    void printBasicBlock(MachineBasicBlock&);
    void printInstruction(MachineInstr&);
    void printOperand(MachineOperand&);
    void printReg(Register);
};
}  // namespace codegen
