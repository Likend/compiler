#include "codegen/MachineInstr.hpp"

#include "codegen/MachineBasicBlock.hpp"
#include "codegen/MachineFunction.hpp"
#include "codegen/MachineOperand.hpp"
#include "util/assert.hpp"
#include "util/IntrusiveList.hpp"

using namespace codegen;

MachineInstr::MachineInstr(
    const MachineInstrDesc&                      desc,
    std::initializer_list<MachineOperandContent> initializer)
    : desc(desc) {
    ops.reserve(desc.operandsNum);
    ASSERT(initializer.size() == desc.operandsNum);

    for (const MachineOperandContent& c : initializer) {
        ops.emplace_back(c, *this);
    }
}

MachineInstr::~MachineInstr() = default;

unsigned MachineInstr::getOperandNo(const_mop_iterator op) const {
    return op - operands_begin();
}

void MachineInstrNode::link_between(MachineInstrNode* curr,
                                    MachineInstrNode* prev,
                                    MachineInstrNode* next) {
    IntrusiveNodeWithParent<MachineBasicBlock>::link_between(curr, prev, next);
    auto* self = static_cast<MachineInstr*>(curr);

    MachineFunction* mf = curr->parent()->parent();
    for (MachineOperand& op : self->ops) {
        if (self->operandIsDef(op))
            mf->addDef(op);
        else if (self->operandIsUse(op))
            mf->addUse(op);
    }
}

void MachineInstrNode::unlink(MachineInstrNode* curr) {
    auto* self = static_cast<MachineInstr*>(curr);

    if (MachineBasicBlock* mbb = curr->parent()) {
        if (MachineFunction* mf = mbb->parent()) {
            for (MachineOperand& op : self->ops) {
                if (self->operandIsDef(op))
                    mf->removeDef(op);
                else if (self->operandIsUse(op))
                    mf->removeUse(op);
            }
        }
    }

    IntrusiveNodeWithParent<MachineBasicBlock>::unlink(curr);
}
