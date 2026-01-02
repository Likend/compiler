#pragma once

#include "codegen/MachineBasicBlock.hpp"
#include "codegen/MachineFunction.hpp"
#include "codegen/MachineFunctionPass.hpp"
#include "codegen/MachineInstr.hpp"
#include "codegen/MachineOperand.hpp"
#include "codegen/Register.hpp"

namespace codegen {
class MultiplyOptimizer {
   public:
    MultiplyOptimizer(MachineInstr& position, Register dest, Register src,
                      int64_t imm)
        : mi(position),
          mbb(*position.parent()),
          mf(*mbb.parent()),
          dest(dest),
          src(src),
          imm(imm) {}

    bool emitOpt();

   private:
    MachineInstr&      mi;
    MachineBasicBlock& mbb;
    MachineFunction&   mf;
    Register           dest;
    Register           src;
    int64_t            imm;

    // Canonical Signed Digit
    struct Term {
        int sign;  // +1 or -1
        int shift;
    };
    static std::vector<Term> findCanonicalSignedDigit(int64_t c);

    void emitMipsMulChain(Register dest, Register src,
                          const std::vector<Term>& terms);
};

class DivisionOptimizer {
   public:
    DivisionOptimizer(MachineInstr& position, Register dest, Register src,
                      int64_t imm)
        : mi(position),
          mbb(*position.parent()),
          mf(*mbb.parent()),
          dest(dest),
          src(src),
          imm(imm) {}

    bool emitOpt();

   private:
    MachineInstr&      mi;
    MachineBasicBlock& mbb;
    MachineFunction&   mf;
    Register           dest;
    Register           src;
    int64_t            imm;

    void emitMipsDivPowerOfTwo(Register dest, Register src, int n);
    void emitMipsDivMagicNumber();
};
}  // namespace codegen
