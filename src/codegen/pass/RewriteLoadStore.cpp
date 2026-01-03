#include "codegen/pass/RewriteLoadStore.hpp"

#include "codegen/MachineBasicBlock.hpp"
#include "codegen/MachineFunction.hpp"
#include "codegen/MachineFunctionPass.hpp"
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
        for (auto miIt = mbb.begin(); miIt != mbb.end();) {
            MachineInstr& mi = *miIt++;
            if (mi.getOpcode() == DESC_LW.opcode ||
                mi.getOpcode() == DESC_SW.opcode) {
                changed |= propagateAddi(mi);
                changed |= propagateFrame(mi);
            }
        }
    }

    return changed;
}

bool RewriteLoadStorePass::propagateAddi(MachineInstr& mi) {
    Register base = mi.getOperand(1).getRegister();
    if (!base.isVirtual()) return false;

    RegisterInfo& info = mi.parent()->parent()->getRegisterInfo(base);
    ASSERT(info.defs() == 1);

    MachineInstr& defInst = (*info.def_begin())->getParent();
    if (defInst.getOpcode() == DESC_ADDI.opcode) {
        int64_t offset = mi.getOperand(2).getImmediate() +
                         defInst.getOperand(2).getImmediate();
        if (offset >= -(1 << 16) && offset < (1 << 16)) {
            mi.changeOperand(mi.getOperand(2), {ImmediateOpKind{offset}});
            mi.changeOperand(mi.getOperand(1),
                             {defInst.getOperand(1).getRegister()});
            return true;
        }
    }
    return false;
}

bool RewriteLoadStorePass::propagateFrame(MachineInstr& mi) {
    Register base = mi.getOperand(1).getRegister();
    if (!base.isVirtual()) return false;

    MachineBasicBlock& mbb = *mi.parent();

    RegisterInfo& info = mi.parent()->parent()->getRegisterInfo(base);
    ASSERT(info.defs() == 1);

    MachineInstr& defInst = (*info.def_begin())->getParent();
    if (defInst.getOpcode() == DESC_FRAME.opcode) {
        const MachineInstrDesc& desc = mi.getOpcode() == DESC_LW.opcode
                                           ? DESC_LOAD_FRAME
                                           : DESC_STORE_FRAME;
        mbb.emplace(mi, desc, mi.getOperand(0).getRegister(),
                    ImmediateOpKind{defInst.getOperand(1).getImmediate()},
                    ImmediateOpKind{mi.getOperand(2).getImmediate()});
        mbb.erase(mi);
        return true;
    }
    return false;
}
