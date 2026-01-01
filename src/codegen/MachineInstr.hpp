#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <vector>

#include "codegen/MachineOperand.hpp"
#include "util/IntrusiveList.hpp"
#include "util/iterator_range.hpp"

namespace codegen {
class MachineOperand;
class MachineBasicBlock;
class MachineFunction;

struct MachineInstrDesc {
    unsigned         opcode;
    std::string_view name;

    // operandsNum = explicitDefsNum + explicitUsesNum + explicitOtherNum
    unsigned operandsNum;

    unsigned explicitDefsNum;
    unsigned explicitUsesNum;
    unsigned explicitOtherNum;
    // unsigned implicitDefsNum;
    // unsigned implicitUsesNum;
};

class MachineInstrNode : public IntrusiveNodeWithParent<MachineBasicBlock> {
    template <typename T>
    friend struct ::IntrusiveNodeTraits;

   public:
    MachineInstrNode() = default;

   private:
    MachineInstrNode(MachineBasicBlock* parent)
        : IntrusiveNodeWithParent<MachineBasicBlock>(parent) {}

    using intrusive_node_type = MachineInstrNode;

    static MachineInstrNode* next(MachineInstrNode* curr) {
        return static_cast<MachineInstrNode*>(
            IntrusiveNodeWithParent<MachineBasicBlock>::next(curr));
    }

    static MachineInstrNode* prev(MachineInstrNode* curr) {
        return static_cast<MachineInstrNode*>(
            IntrusiveNodeWithParent<MachineBasicBlock>::prev(curr));
    }

    static void link_between(MachineInstrNode* curr, MachineInstrNode* prev,
                             MachineInstrNode* next);

    static void unlink(MachineInstrNode* curr);

    static MachineInstrNode* create_sentinel(MachineBasicBlock& parent) {
        return new MachineInstrNode{&parent};
    }
};

class MachineInstr : public MachineInstrNode {
    friend class MachineInstrNode;
    // Operands 顺序：@see MCInstrDesc
    // 1. explicitDefsNum 个 explicit defs (reg)
    // 2. explicitUsesNum 个 explicit uses (reg)
    // 3. explicitOtherNum 个 other explicit operands (immediate, address)
    // 4. implicitDefsNum 个 implicit defs (暂时没有)
    // 5. implicitUsesNum 个 implicit uses (暂时没有)
    std::vector<MachineOperand> ops;

   public:
    const MachineInstrDesc& desc;
    std::string             annotation;

    MachineInstr(const MachineInstrDesc& desc,
                 std::initializer_list<MachineOperandContent>);
    template <typename... OperandArgT>
    MachineInstr(const MachineInstrDesc& desc, OperandArgT... args)
        : MachineInstr(desc, {MachineOperandContent{args}...}) {}

    ~MachineInstr();

    uint16_t              getOpcode() const { return desc.opcode; }
    size_t                getNumOperands() const { return desc.operandsNum; }
    MachineOperand&       getOperand(size_t i) { return ops[i]; }
    const MachineOperand& getOperand(size_t i) const { return ops[i]; }

    using mop_iterator       = MachineOperand*;
    using const_mop_iterator = const MachineOperand*;
    using mop_range          = iterator_range<mop_iterator>;
    using const_mop_range    = iterator_range<const_mop_iterator>;

    mop_iterator       operands_begin() { return &ops[0]; }
    mop_iterator       operands_end() { return &ops[desc.operandsNum]; }
    const_mop_iterator operands_begin() const { return &ops[0]; }
    const_mop_iterator operands_end() const { return &ops[desc.operandsNum]; }
    mop_range          operands() { return {operands_begin(), operands_end()}; }
    const_mop_range    operands() const {
        return {operands_begin(), operands_end()};
    }

    mop_range explicit_defs() { return {&ops[0], &ops[desc.explicitDefsNum]}; }
    const_mop_range explicit_defs() const {
        return {&ops[0], &ops[desc.explicitDefsNum]};
    }
    mop_range explicit_uses() {
        return {&ops[desc.explicitDefsNum],
                &ops[desc.explicitDefsNum + desc.explicitUsesNum]};
    }
    const_mop_range explicit_uses() const {
        return {&ops[desc.explicitDefsNum],
                &ops[desc.explicitDefsNum + desc.explicitUsesNum]};
    }
    mop_range explicit_others() {
        return {&ops[desc.explicitDefsNum + desc.explicitUsesNum],
                &ops[desc.operandsNum]};
    }
    const_mop_range explicit_others() const {
        return {&ops[desc.explicitDefsNum + desc.explicitUsesNum],
                &ops[desc.operandsNum]};
    }
    mop_range explicit_regs() {
        return {&ops[0], &ops[desc.explicitDefsNum + desc.explicitUsesNum]};
    }
    const_mop_range explicit_regs() const {
        return {&ops[0], &ops[desc.explicitDefsNum + desc.explicitUsesNum]};
    }

    unsigned getOperandNo(const_mop_iterator op) const;

    bool operandNoIsDef(unsigned no) const {
        return no >= 0 && no < desc.explicitDefsNum;
    }
    bool operandNoIsUse(unsigned no) const {
        return no >= desc.explicitDefsNum &&
               no < desc.explicitDefsNum + desc.explicitUsesNum;
    }

    bool operandIsDef(const MachineOperand& op) const {
        return operandNoIsDef(getOperandNo(&op));
    }
    bool operandIsUse(const MachineOperand& op) const {
        return operandNoIsUse(getOperandNo(&op));
    }

    void addAnnotation(std::string_view ann) {
        annotation = annotation.append(ann);
        annotation = annotation.append("; ");
    }

    void changeOperand(const MachineOperand& opPosition, MachineOperandContent);

    MachineFunction* getFunction();
};

#define HANDLE_DESC_DEF(opcode, name, def, use, other)               \
    [[maybe_unused]] constexpr static MachineInstrDesc DESC_##name { \
        opcode, #name, (def) + (use) + (other), def, use, other      \
    }
#include "codegen/DescDefs.hpp"
}  // namespace codegen
