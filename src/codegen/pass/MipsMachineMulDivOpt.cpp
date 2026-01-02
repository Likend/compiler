#include "codegen/pass/MipsMachineMulDivOpt.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <utility>
#include <vector>

#include "codegen/MachineBasicBlock.hpp"
#include "codegen/MachineFunction.hpp"
#include "codegen/MachineInstr.hpp"
#include "codegen/MachineOperand.hpp"
#include "codegen/Register.hpp"
#include "util/bits.hpp"

using namespace codegen;

// 注意，这里的 DESC_ADD, DESC_ADDI 实际上会被替换成无符号整数版本

std::vector<MultiplyOptimizer::Term>
MultiplyOptimizer::findCanonicalSignedDigit(int64_t c) {
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

void MultiplyOptimizer::emitMipsMulChain(Register dest, Register src,
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

bool MultiplyOptimizer::emitOpt() {
    std::vector<Term> terms = findCanonicalSignedDigit(imm);
    if (terms.size() <= 3) {
        emitMipsMulChain(mi.getOperand(0).getRegister(),
                         mi.getOperand(1).getRegister(), terms);
        return true;
    } else {
        // Register liReg = mf.CreateVReg();
        // mbb.emplace(mi, DESC_LI, liReg, ImmediateOpKind(imm));
        // mbb.emplace(mi, DESC_MUL, mi.getOperand(0).getRegister(),
        //             mi.getOperand(1).getRegister(), liReg);
        // return true;
        return false;
    }
}

struct Magic {
    int32_t M;  // 魔数
    int32_t s;  // 移位数
};

Magic calculateMagic(int32_t d) {
    uint32_t ad  = std::abs(d);
    uint32_t anc = (1U << 31) - 1 - (1U << 31) % ad;
    int32_t  p   = 31;
    uint32_t q1  = (1U << 31) / anc;
    uint32_t r1  = (1U << 31) % anc;
    uint32_t q2  = (1U << 31) / ad;
    uint32_t r2  = (1U << 31) % ad;
    uint32_t delta;

    do {
        p++;
        q1 = 2 * q1;
        r1 = 2 * r1;
        if (r1 >= anc) {
            q1++;
            r1 -= anc;
        }
        q2 = 2 * q2;
        r2 = 2 * r2;
        if (r2 >= ad) {
            q2++;
            r2 -= ad;
        }
        delta = ad - r2;
    } while (q1 < delta || (q1 == delta && r1 == 0));

    // Magic mag;
    // mag.M = (int32_t)(q2 + 1);
    // if (d < 0) mag.M = -mag.M;  // 如果除数是负数
    // mag.s = p - 32;
    // return mag;
    Magic mag;
    // 强制转换为 uint32_t 处理，避免溢出定义的混乱
    uint32_t q_final = q2 + 1;
    mag.M            = static_cast<int32_t>(q_final);
    mag.s            = p - 32;
    return mag;
}

void DivisionOptimizer::emitMipsDivPowerOfTwo(Register dest, Register src,
                                              int n) {
    // 针对除以 imm = 2^n 的优化，对于负数，我们需要加一个偏执值（Bias）
    // bias = (x < 0) ? (x + 2^n - 1) : x
    // 然后再进行右移。
    int64_t  bias = (1 << n) - 1;
    Register t0   = mf.CreateVReg();
    Register t1   = mf.CreateVReg();
    Register t2   = mf.CreateVReg();
    // 提取符号位：如果是正数则 $t0=0，如果是负数则 $t0=-1 (0xFFFFFFFF)
    // sra  $t0, $src, 31
    mbb.emplace(mi, DESC_SRA, t0, src, ImmediateOpKind{31});
    // 如果是正数则 $t0=0，如果是负数则 $t0=`imm-1`
    // and(i) $t1, $t0, `imm-1`
    if (imm <= (1 << 16)) {
        mbb.emplace(mi, DESC_ANDI, t1, t0, ImmediateOpKind{bias});
    } else {
        Register temp = mf.CreateVReg();
        mbb.emplace(mi, DESC_LI, temp, ImmediateOpKind{bias});
        mbb.emplace(mi, DESC_AND, t1, t0, temp);
    }
    // x + bias
    // add $t2, $t1, $src
    mbb.emplace(mi, DESC_ADD, t2, t1, src);
    // (x + bias) >> `n`
    // sra $dest, $t2, `n`
    mbb.emplace(mi, DESC_SRA, dest, t2, ImmediateOpKind{n});
}

void DivisionOptimizer::emitMipsDivMagicNumber() {
    Magic magic = calculateMagic(static_cast<int32_t>(std::abs(imm)));

    Register t0 = mf.CreateVReg();  // 存放 M
    Register t1 = mf.CreateVReg();  // 存放 HI 的结果
    Register t2 = mf.CreateVReg();  // 存放符号位
    // 加载魔数到寄存器
    // li $t0, M
    mbb.emplace(mi, DESC_LI, t0, ImmediateOpKind{magic.M});
    // 有符号乘法：$src * M
    // 结果的高 32 位存放在 HI，低 32 位存放在 LO
    mbb.emplace(mi, DESC_MULT, src, t0);
    mbb.emplace(mi, DESC_MFHI, t1);
    if (magic.M < 0) {
        Register t_add = mf.CreateVReg();
        mbb.emplace(mi, DESC_ADD, t_add, t1, src);
        t1 = t_add;
    }

    // 执行额外的算术右移 (如果 s > 0)
    // 有些常数计算出 s=0，则跳过此步
    if (magic.s > 0) {
        Register t1_shifted = mf.CreateVReg();
        mbb.emplace(mi, DESC_SRA, t1_shifted, t1, ImmediateOpKind{magic.s});
        t1 = t1_shifted;
    }

    // 提取原被除数的符号位
    // 如果 src < 0，$t2 = -1 (0xFFFFFFFF)；如果 src >= 0，$t2 = 0
    mbb.emplace(mi, DESC_SRA, t2, src, ImmediateOpKind{31});

    Register tempRes;
    if (imm < 0) {
        tempRes = mf.CreateVReg();
    } else {
        tempRes = dest;
    }

    // 最终修正：商 = 结果 - 符号位
    // 由于负数的符号位是 -1，减去 -1 等于加 1，正好实现了向零取整
    mbb.emplace(mi, DESC_SUB, tempRes, t1, t2);

    if (imm < 0) {
        mbb.emplace(mi, DESC_SUB, dest, REG_ZERO, tempRes);
    }
}

bool DivisionOptimizer::emitOpt() {
    if (imm == 1) {
        mbb.emplace(mi, DESC_MOVE, dest, src);
        return true;
    } else if (imm == -1) {
        mbb.emplace(mi, DESC_SUB, dest, REG_ZERO, src);
        return true;
    } else if (imm == 0) {
        mbb.emplace(mi, DESC_SDIV, dest, src, REG_ZERO);
        return true;
    } else if (popcount(static_cast<uint64_t>(std::abs(imm))) == 1) {
        int n = countr_zero(static_cast<uint64_t>(imm));

        if (imm > 0) {
            emitMipsDivPowerOfTwo(dest, src, n);
        } else {
            Register temp = mf.CreateVReg();
            emitMipsDivPowerOfTwo(temp, src, n);
            mbb.emplace(mi, DESC_SUB, dest, REG_ZERO, temp);
        }
        return true;
    } else {
        emitMipsDivMagicNumber();
        return true;
    }
}
