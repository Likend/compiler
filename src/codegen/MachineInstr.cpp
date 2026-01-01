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

void MachineInstr::changeOperand(const MachineOperand& opPosition,
                                 MachineOperandContent c) {
    auto&            pos = const_cast<MachineOperand&>(opPosition);
    MachineFunction* mf  = getFunction();

    if (operandIsDef(pos)) {
        if (mf) mf->removeDef(pos);
    } else if (operandIsUse(pos)) {
        if (mf) mf->removeUse(pos);
    }

    pos = MachineOperand{c, *this};

    if (operandIsDef(pos)) {
        if (mf) mf->addDef(pos);
    } else if (operandIsUse(pos)) {
        if (mf) mf->addUse(pos);
    }
}

MachineFunction* MachineInstr::getFunction() {
    if (MachineBasicBlock* mbb = parent())
        return mbb->parent();
    else
        return nullptr;
}

void MachineInstrNode::link_between(MachineInstrNode* curr,
                                    MachineInstrNode* prev,
                                    MachineInstrNode* next) {
    IntrusiveNodeWithParent<MachineBasicBlock>::link_between(curr, prev, next);
    auto* self = static_cast<MachineInstr*>(curr);

    MachineFunction* mf = self->getFunction();
    for (MachineOperand& op : self->ops) {
        if (self->operandIsDef(op))
            mf->addDef(op);
        else if (self->operandIsUse(op))
            mf->addUse(op);
    }
}

void MachineInstrNode::unlink(MachineInstrNode* curr) {
    auto* self = static_cast<MachineInstr*>(curr);

    if (MachineFunction* mf = self->getFunction()) {
        for (MachineOperand& op : self->ops) {
            if (self->operandIsDef(op))
                mf->removeDef(op);
            else if (self->operandIsUse(op))
                mf->removeUse(op);
        }
    }

    IntrusiveNodeWithParent<MachineBasicBlock>::unlink(curr);
}
