#pragma once

#include <ostream>

#include "codegen/Register.hpp"
#include "ir/GlobalValue.hpp"
#include "ir/Module.hpp"
#include "ir/Pass.hpp"

namespace codegen {
class MachineFunction;
class MachineBasicBlock;
class MachineInstr;
class MachineOperand;

class MipsPrinterPass final : public ir::ImmutablePass {
   public:
    MipsPrinterPass(std::ostream& os) : os(os) {}

    bool doInitialization(ir::Module&) override;

   private:
    std::ostream& os;

    void printGlobalVariable(ir::GlobalVariable&);
    void printFunction(MachineFunction&);
    void printBasicBlock(MachineBasicBlock&);
    void printInstruction(MachineInstr&);
    void printOperand(MachineOperand&);
    void printReg(Register reg) { printReg(reg, os); }

   public:
    static void printReg(Register, std::ostream&);
};
}  // namespace codegen
