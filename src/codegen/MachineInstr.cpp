#include "codegen/MachineInstr.hpp"

#include "codegen/MachineBasicBlock.hpp"
#include "codegen/MachineFunction.hpp"
#include "codegen/MachineOperand.hpp"
#include "util/assert.hpp"

using namespace codegen;

MachineInstr::MachineInstr(
    MachineBasicBlock& parent, const MachineInstrDesc& desc,
    std::initializer_list<MachineOperandContent> initializer)
    : parent(parent), desc(desc) {
    ops.reserve(desc.operandsNum);

    MachineFunction& mf = parent.getParent();
    ASSERT(initializer.size() == desc.operandsNum);
    size_t i = 0;
    for (const MachineOperandContent& c : initializer) {
        auto& p = ops.emplace_back(c, *this);
        if (operandNoIsDef(i))
            mf.addDef(p);
        else if (operandNoIsUse(i))
            mf.addUse(p);
        i++;
    }
}

MachineInstr::~MachineInstr() = default;

unsigned MachineInstr::getOperandNo(const_mop_iterator op) const {
    return op - operands_begin();
}
