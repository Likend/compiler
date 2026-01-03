#include "codegen/pass/RemoveJump.hpp"

#include "codegen/MachineBasicBlock.hpp"
#include "codegen/MachineFunction.hpp"
#include "codegen/MachineInstr.hpp"
#include "util/IntrusiveList.hpp"

using namespace ir;
using namespace codegen;

bool RemoveJumpPass::runOnMachineFunction(MachineFunction& mf) {
    bool changed = false;
    for (MachineBasicBlock& mbb : mf) {
        if (mbb.empty()) continue;
        MachineInstr& back = mbb.back();
        if (back.getOpcode() == DESC_JUMP.opcode) {
            MachineBasicBlock* target =
                back.getOperand(0).getMachineBasicBlock();
            if (&mbb != &mf.back() &&
                target == &*++IntrusiveIterator<MachineBasicBlock>{mbb}) {
                mbb.erase(back);
                changed = true;
            }
        }
    }
    return changed;
}
