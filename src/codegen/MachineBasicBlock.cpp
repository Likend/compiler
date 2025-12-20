#include "codegen/MachineBasicBlock.hpp"

#include "codegen/MachineFunction.hpp"
#include "codegen/MachineInstr.hpp"
#include "util/assert.hpp"

using namespace codegen;

MachineBasicBlock::iterator MachineBasicBlock::erase(iterator it) {
    MachineFunction& mf = it->getParent().getParent();
    for (auto& def : it->explicit_defs()) {
        RegisterInfo& info = mf.getRegisterInfo(def.getRegister());
        info.removeDef(def);
        if (info.def_empty()) {
            ASSERT_WITH(info.use_empty(), "cannot remove use not empty");
            mf.regInfos.erase(def.getRegister());
        }
    }
    for (auto& use : it->explicit_uses()) {
        RegisterInfo& info = mf.getRegisterInfo(use.getRegister());
        info.removeUse(use);
    }

    return {instrs.erase(it.origin_it())};
}
