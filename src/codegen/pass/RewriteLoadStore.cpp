#include "codegen/pass/RewriteLoadStore.hpp"

#include "codegen/MachineBasicBlock.hpp"
#include "codegen/MachineFunction.hpp"
#include "codegen/MachineInstr.hpp"
#include "codegen/MachineOperand.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterInfo.hpp"
#include "util/assert.hpp"

using namespace ir;
using namespace codegen;

bool RewriteLoadStorePass::runOnMachineFunction(MachineFunction& mf) {
    bool changed = false;

    for (MachineBasicBlock& mbb : mf) {
        for (MachineInstr& mi : mbb) {
            if (mi.getOpcode() == DESC_LW.opcode ||
                mi.getOpcode() == DESC_SW.opcode) {
                Register base = mi.getOperand(1).getRegister();
                if (!base.isVirtual()) continue;

                RegisterInfo& info = mf.getRegisterInfo(base);
                ASSERT(info.defs() == 1);

                MachineInstr& defInst = (*info.def_begin())->getParent();
                if (!(defInst.getOpcode() == DESC_ADDI.opcode)) continue;

                int64_t offset = mi.getOperand(2).getImmediate() +
                                 defInst.getOperand(2).getImmediate();
                if (offset >= -(1 << 16) && offset < (1 << 16)) {
                    mi.changeOperand(mi.getOperand(2),
                                     {ImmediateOpKind{offset}});
                    mi.changeOperand(mi.getOperand(1),
                                     {defInst.getOperand(1).getRegister()});
                    changed = true;
                }
            }
        }
    }

    return changed;
}
