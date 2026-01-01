#include "codegen/MipsMachineMulDivOpt.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

#include "codegen/MachineBasicBlock.hpp"
#include "codegen/MachineFunction.hpp"
#include "codegen/MachineInstr.hpp"
#include "codegen/MachineOperand.hpp"
#include "codegen/Register.hpp"

using namespace codegen;

struct MultiOpt {
    MachineInstr&      mi;
    MachineBasicBlock& mbb;
    MachineFunction&   mf;

    MultiOpt(MachineInstr& mi) : mi(mi), mbb(*mi.parent()), mf(*mbb.parent()) {
        ASSERT(mi.getOpcode() == DESC_MULTI.opcode);
    }

    // Canonical Signed Digit
    struct Term {
        int sign;  // +1 or -1
        int shift;
    };

    static std::vector<Term> findCanonicalSignedDigit(int64_t c) {
        std::vector<Term> results;
        while (c != 0 && results.size() < 3) {
            // 找到最接近 C 的 2^a
            int     a    = static_cast<int>(std::round(std::log2(std::abs(c))));
            int64_t pow2 = 1LL << a;

            int sign = (c > 0) ? 1 : -1;
            // 检查方向：是靠近还是远离

            results.push_back({sign, a});
            c -= (sign * pow2);
        }
        // reorder
        // 第一项最好为正
        auto it = std::find_if(results.begin(), results.end(),
                               [](const Term& term) { return term.sign == 1; });
        if (it != results.end() && it != results.begin())
            std::swap(results.front(), *it);
        return results;
    }

    void emitMipsMulChain(Register dest, Register src,
                          const std::vector<Term>& terms) {
        Register accReg = REG_ZERO;

        for (size_t i = 0; i < terms.size(); i++) {
            Register tmp;
            // tmp = src << shift
            if (terms[i].shift > 0) {
                tmp = mf.CreateVReg();
                mbb.emplace(mi, DESC_SLL, tmp, src,
                            ImmediateOpKind{terms[i].shift});
            } else {  // tmp = src << 0 = src
                tmp = src;
            }

            if (i == 0) {
                if (terms[i].sign == 1) {
                    accReg = tmp;  // 第一项为正，直接作为基准
                } else {
                    // 第一项为负，强制使用 subu $acc, $zero, $tmp
                    accReg = mf.CreateVReg();
                    mbb.emplace(mi, DESC_SUB, accReg, REG_ZERO, tmp);
                }
            } else {
                Register                nextAcc = mf.CreateVReg();
                const MachineInstrDesc& op =
                    (terms[i].sign == 1) ? DESC_ADD : DESC_SUB;
                mbb.emplace(mi, op, nextAcc, accReg, tmp);
                accReg = nextAcc;
            }
        }
        mbb.emplace(mi, DESC_MOVE, dest, accReg);
    }

    void emitOpt() {
        int64_t           c     = mi.getOperand(2).getImmediate();
        std::vector<Term> terms = findCanonicalSignedDigit(c);
        if (terms.size() <= 3) {
            emitMipsMulChain(mi.getOperand(0).getRegister(),
                             mi.getOperand(1).getRegister(), terms);
        } else {
            Register liReg = mf.CreateVReg();
            mbb.emplace(mi, DESC_LI, liReg, ImmediateOpKind(c));
            mbb.emplace(mi, DESC_MUL, mi.getOperand(0).getRegister(),
                        mi.getOperand(1).getRegister(), liReg);
        }
        mbb.erase(mi);
    }
};

bool MipsMachineMulDivOpt::runOnMachineFunction(MachineFunction& mf) {
    bool changed = false;
    for (MachineBasicBlock& mbb : mf) {
        for (auto miIt = mbb.begin(); miIt != mbb.end();) {
            MachineInstr& mi = *miIt++;
            if (mi.getOpcode() == DESC_MULTI.opcode) {
                MultiOpt{mi}.emitOpt();
                changed = true;
            }
        }
    }
    return changed;
}
