#include "codegen/MovePropagation.hpp"

#include "codegen/MachineBasicBlock.hpp"
#include "codegen/MachineFunction.hpp"
#include "codegen/MachineInstr.hpp"
#include "codegen/MachineOperand.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterInfo.hpp"
#include "util/assert.hpp"

using namespace codegen;

bool MovePropagationPass::runOnMachineFunction(MachineFunction& mf) {
    bool changed = false;
    for (MachineBasicBlock& mbb : mf) {
        for (auto miIt = mbb.begin(); miIt != mbb.end();) {
            MachineInstr& mi = *miIt++;
            if (mi.getOpcode() == DESC_MOVE.opcode) {
                Register     srcReg = mi.getOperand(1).getRegister();
                RegisterInfo destRegInfo =
                    mf.getRegisterInfo(mi.getOperand(0).getRegister());
                ASSERT_WITH(destRegInfo.defs() == 1,
                            "Move Propagation should use in SSA form.");
                for (MachineOperand* use : destRegInfo.use()) {
                    MachineInstr& useInst = use->getParent();
                    useInst.changeOperand(*use, {srcReg});
                }
                changed = true;
                mbb.erase(mi);
            }
        }
    }
    return changed;
}
