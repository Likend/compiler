#include "codegen/pass/ConstantPropagation.hpp"

#include <queue>
#include <unordered_map>

#include "codegen/MachineBasicBlock.hpp"
#include "codegen/MachineFunction.hpp"
#include "codegen/MachineInstr.hpp"
#include "codegen/MachineOperand.hpp"
#include "codegen/Register.hpp"
#include "util/assert.hpp"

using namespace codegen;

struct Lattice {
    enum Type { None, Bottom, Constant };
    Type    type     = None;
    int64_t constant = 0;
};

struct LatticeAnalysis : std::unordered_map<Register, Lattice> {};

Lattice evaluate(MachineInstr& mi, const LatticeAnalysis& lattices);
bool    rewriteInstruction(MachineInstr& mi, const LatticeAnalysis& lattices);

bool ConstantPropagation::runOnMachineFunction(MachineFunction& mf) {
    if (mf.empty()) return false;

    LatticeAnalysis lattices;
    for (const auto& it : mf.regInfos) {
        lattices[it.first] = {};
    }

    std::queue<MachineInstr*> workList;
    MachineBasicBlock&        entryBB = mf.front();
    for (MachineInstr& mi : entryBB) {
        workList.push(&mi);
    }

    while (!workList.empty()) {
        MachineInstr* mi = workList.front();
        workList.pop();

        // 指令只有一个 Def (SSA 特性)
        if (mi->desc.explicitDefsNum == 0) continue;
        Register destReg = mi->getOperand(0).getRegister();

        const Lattice& oldLattice = lattices.at(destReg);
        const Lattice& newLattice = evaluate(*mi, lattices);

        // 如果 Lattice 状态发生了升级 (None -> Constant -> Bottom)
        if (oldLattice.type != newLattice.type) {
            lattices[destReg] = newLattice;

            // 将所有使用该寄存器的指令重新加入 workList
            for (auto* user : mf.getRegisterInfo(destReg).use()) {
                MachineInstr& userMi = user->getParent();
                workList.push(&userMi);
            }
        }
    }

    // 替换指令
    bool changed = false;
    for (MachineBasicBlock& mbb : mf) {
        for (auto it = mbb.begin(); it != mbb.end();) {
            MachineInstr& mi = *it++;
            changed |= rewriteInstruction(mi, lattices);
        }
    }

    return changed;
}

struct LatticeOperatorBase {
    LatticeOperatorBase(MachineInstr& mi, const LatticeAnalysis& lattices)
        : mi(mi), mbb(*mi.parent()), mf(*mbb.parent()), lattices(lattices) {}

    MachineInstr&      mi;
    MachineBasicBlock& mbb;
    MachineFunction&   mf;

    const LatticeAnalysis& lattices;
};

struct LatticeOperatorBaseRegImm : LatticeOperatorBase {
    LatticeOperatorBaseRegImm(MachineInstr& mi, const LatticeAnalysis& lattices)
        : LatticeOperatorBase(mi, lattices),
          dest(mi.getOperand(0).getRegister()),
          src(mi.getOperand(1).getRegister()),
          imm(mi.getOperand(2).getImmediate()),
          srcLat(lattices.at(src)) {}

    Register       dest;
    Register       src;
    int64_t        imm;
    const Lattice& srcLat;
};

struct LatticeOperatorBaseRegReg : LatticeOperatorBase {
    LatticeOperatorBaseRegReg(MachineInstr& mi, const LatticeAnalysis& lattices)
        : LatticeOperatorBase(mi, lattices),
          dest(mi.getOperand(0).getRegister()),
          src1(mi.getOperand(1).getRegister()),
          src2(mi.getOperand(2).getRegister()),
          l1(lattices.at(src1)),
          l2(lattices.at(src2)) {}

    Register dest;
    Register src1;
    Register src2;

    const Lattice& l1;
    const Lattice& l2;
};

template <unsigned Opcode>
struct RewriteInstruction : LatticeOperatorBase {
    using LatticeOperatorBase::LatticeOperatorBase;
    bool operator()() { return false; }
};

template <>
struct RewriteInstruction<DESC_ADD.opcode> : LatticeOperatorBaseRegReg {
    using LatticeOperatorBaseRegReg::LatticeOperatorBaseRegReg;
    bool operator()() {
        if (l1.type == Lattice::Constant || l2.type == Lattice::Constant) {
            int64_t val =
                (l1.type == Lattice::Constant) ? l1.constant : l2.constant;
            Register src = (l1.type == Lattice::Constant) ? src2 : src1;

            if (val >= -32768 && val <= 32767) {
                mbb.emplace(mi, DESC_ADDI, dest, src, ImmediateOpKind{val});
                mbb.erase(mi);
                return true;
            }
        }
        return false;
    }
};

template <>
struct RewriteInstruction<DESC_SUB.opcode> : LatticeOperatorBaseRegReg {
    using LatticeOperatorBaseRegReg::LatticeOperatorBaseRegReg;
    bool operator()() {
        if (l2.type == Lattice::Constant) {
            int64_t val    = l2.constant;
            int64_t negVal = -val;

            if (negVal >= -32768 && negVal <= 32767) {
                mbb.emplace(mi, DESC_ADDI, dest, src1, ImmediateOpKind{negVal});
                mbb.erase(mi);
                return true;
            }
        }
        return false;
    }
};

template <>
struct RewriteInstruction<DESC_MUL.opcode> : LatticeOperatorBaseRegReg {
    using LatticeOperatorBaseRegReg::LatticeOperatorBaseRegReg;

    bool operator()() {
        if (l1.type == Lattice::Constant || l2.type == Lattice::Constant) {
            int64_t val =
                (l1.type == Lattice::Constant) ? l1.constant : l2.constant;
            Register src = (l1.type == Lattice::Constant) ? src2 : src1;

            mbb.emplace(mi, DESC_MULTI, dest, src, ImmediateOpKind{val});
            mbb.erase(mi);
            return true;
        }
        return false;
    }
};

// template <>
// struct RewriteInstruction<DESC_SDIV.opcode> : LatticeOperatorBaseRegReg {
//     using LatticeOperatorBaseRegReg::LatticeOperatorBaseRegReg;

//     bool operator()() {
//         if (l2.type == Lattice::Constant) {
//             int64_t val    = l2.constant;
//             int64_t negVal = -val;

//             mbb.emplace(mi, DESC_SDIVI, dest, src1, ImmediateOpKind{negVal});
//             mbb.erase(mi);
//             return true;
//         }
//         return false;
//     }
// };

bool rewriteInstruction(MachineInstr& mi, const LatticeAnalysis& lattices) {
    MachineBasicBlock& mbb = *mi.parent();

    // 检查是否可以完全折叠为 li
    if (mi.desc.explicitDefsNum > 0) {
        Register       destReg = mi.getOperand(0).getRegister();
        const Lattice& destLat = lattices.at(destReg);

        if (destLat.type == Lattice::Constant) {
            if (mi.getOpcode() == DESC_LI.opcode) return false;

            // 将指令替换为 LI $dest, constant
            // 注意：如果常量超过 16 位，MIPS 汇编器通常会将 li 宏展开为 lui +
            // ori
            mbb.emplace(mi, DESC_LI, destReg,
                        ImmediateOpKind{destLat.constant});
            mbb.erase(mi);
            return true;
        }
    }

    switch (mi.getOpcode()) {
#define HANDLE_DESC_DEF(opcode, name, def, use, other) \
    case opcode:                                       \
        return RewriteInstruction<opcode>{mi, lattices}();
#include "codegen/DescDefs.hpp"
        default:
            UNREACHABLE();
    }
}

/// === LatticeEvaluate ===
template <unsigned Opcode>
struct LatticeEvaluate : LatticeOperatorBase {
    using LatticeOperatorBase::LatticeOperatorBase;
    Lattice operator()() { return {Lattice::Bottom}; }
};

template <>
struct LatticeEvaluate<DESC_LI.opcode> : LatticeOperatorBase {
    using LatticeOperatorBase::LatticeOperatorBase;
    Lattice operator()() {
        return {Lattice::Constant, mi.getOperand(1).getImmediate()};
    }
};

// For instructions with 1 def, 1 use, 1 imm
template <typename Derive>
struct LatticeEvalRegImm : LatticeOperatorBaseRegImm {
    using LatticeOperatorBaseRegImm::LatticeOperatorBaseRegImm;
    Lattice operator()() {
        if (srcLat.type == Lattice::Constant)
            return {Lattice::Constant, Derive::calculate(srcLat.constant, imm)};
        else
            return srcLat;
    }
};

// For instructions with 1 def, 2 uses
template <typename Derive>
struct LatticeEvalRegReg : LatticeOperatorBaseRegReg {
    using LatticeOperatorBaseRegReg::LatticeOperatorBaseRegReg;
    Lattice operator()() {
        if (l1.type == Lattice::Constant && l2.type == Lattice::Constant)
            return {Lattice::Constant,
                    Derive::calculate(l1.constant, l2.constant)};
        else if (l1.type == Lattice::Bottom || l2.type == Lattice::Bottom)
            return {Lattice::Bottom};
        else
            return {Lattice::None};
    }
};

#define HANDLE_CALC_OPERATOR(name, operator)                 \
    struct Calculate##name {                                 \
        static int64_t calculate(int64_t lhs, int64_t rhs) { \
            return lhs operator rhs;                         \
        }                                                    \
    }
HANDLE_CALC_OPERATOR(Add, +);
HANDLE_CALC_OPERATOR(Sub, -);
HANDLE_CALC_OPERATOR(Mul, *);
HANDLE_CALC_OPERATOR(SDiv, /);
HANDLE_CALC_OPERATOR(SRem, %);
HANDLE_CALC_OPERATOR(Xor, ^);
HANDLE_CALC_OPERATOR(Seq, ==);
HANDLE_CALC_OPERATOR(Sne, !=);
HANDLE_CALC_OPERATOR(Sgt, >);
HANDLE_CALC_OPERATOR(Sge, >=);
HANDLE_CALC_OPERATOR(Slt, <);
HANDLE_CALC_OPERATOR(Sle, <=);
HANDLE_CALC_OPERATOR(Sll, <<);
HANDLE_CALC_OPERATOR(Srl, >>);
#undef HANDLE_CALC_OPERATOR

#define HANDLE_LATTICE_EVAL(desc_name, base_name, calc_name)                   \
    template <>                                                                \
    struct LatticeEvaluate<DESC_##desc_name.opcode>                            \
        : LatticeEval##base_name<LatticeEvaluate<DESC_##desc_name.opcode>>,    \
          Calculate##calc_name {                                               \
        using LatticeEval##base_name<                                          \
            LatticeEvaluate<DESC_##desc_name.opcode>>::LatticeEval##base_name; \
    }

HANDLE_LATTICE_EVAL(ADD, RegReg, Add);
HANDLE_LATTICE_EVAL(SDIV, RegReg, SDiv);
HANDLE_LATTICE_EVAL(SREM, RegReg, SRem);
HANDLE_LATTICE_EVAL(XOR, RegReg, Xor);
HANDLE_LATTICE_EVAL(SEQ, RegReg, Seq);
HANDLE_LATTICE_EVAL(SNE, RegReg, Sne);
HANDLE_LATTICE_EVAL(SGT, RegReg, Sgt);
HANDLE_LATTICE_EVAL(SGE, RegReg, Sge);
HANDLE_LATTICE_EVAL(SLT, RegReg, Slt);
HANDLE_LATTICE_EVAL(SLE, RegReg, Sle);

HANDLE_LATTICE_EVAL(ADDI, RegImm, Add);
HANDLE_LATTICE_EVAL(SLL, RegImm, Sll);
HANDLE_LATTICE_EVAL(SRL, RegImm, Srl);

#undef HANDLE_LATTICE_EVAL

// 特化
template <>
struct LatticeEvaluate<DESC_SUB.opcode>
    : LatticeEvalRegReg<LatticeEvaluate<DESC_SUB.opcode>>, CalculateSub {
    using LatticeEvalRegReg<
        LatticeEvaluate<DESC_SUB.opcode>>::LatticeEvalRegReg;

    Lattice operator()() {
        Lattice ret =
            LatticeEvalRegReg<LatticeEvaluate<DESC_SUB.opcode>>::operator()();
        // 特殊优化：如果 $src1 == $src2，结果必为 0
        if (ret.type != Lattice::Constant) {
            if (src1 == src2) return {Lattice::Constant, 0};
        }
        return ret;
    }
};

template <>
struct LatticeEvaluate<DESC_MUL.opcode>
    : LatticeEvalRegReg<LatticeEvaluate<DESC_MUL.opcode>>, CalculateMul {
    using LatticeEvalRegReg<
        LatticeEvaluate<DESC_MUL.opcode>>::LatticeEvalRegReg;

    Lattice operator()() {
        Lattice ret =
            LatticeEvalRegReg<LatticeEvaluate<DESC_MUL.opcode>>::operator()();
        // 特殊优化：如果 $src1 == 0 或 $src2 == 0，结果必为 0
        if (ret.type != Lattice::Constant) {
            if ((l1.type == Lattice::Constant && l1.constant == 0) ||
                (l2.type == Lattice::Constant && l2.constant == 0))
                return {Lattice::Constant, 0};
        }
        return ret;
    }
};

template <>
struct LatticeEvaluate<DESC_MULTI.opcode>
    : LatticeEvalRegImm<LatticeEvaluate<DESC_MULTI.opcode>>, CalculateMul {
    using LatticeEvalRegImm<
        LatticeEvaluate<DESC_MULTI.opcode>>::LatticeEvalRegImm;

    Lattice operator()() {
        Lattice ret =
            LatticeEvalRegImm<LatticeEvaluate<DESC_MULTI.opcode>>::operator()();
        if (ret.type != Lattice::Constant) {
            if ((srcLat.type == Lattice::Constant && srcLat.constant == 0) ||
                imm == 0)
                return {Lattice::Constant, 0};
        }
        return ret;
    }
};

Lattice evaluate(MachineInstr& mi, const LatticeAnalysis& lattices) {
    // 立即数加载指令 (li $t0, 100)
    switch (mi.getOpcode()) {
#define HANDLE_DESC_DEF(opcode, name, def, use, other) \
    case opcode:                                       \
        return LatticeEvaluate<opcode>{mi, lattices}()
#include "codegen/DescDefs.hpp"
        default:
            UNREACHABLE();
    }
}
