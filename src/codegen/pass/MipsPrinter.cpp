#include "codegen/pass/MipsPrinter.hpp"

#include <ostream>
#include <variant>

#include "codegen/MachineBasicBlock.hpp"
#include "codegen/MachineFunction.hpp"
#include "codegen/MachineInstr.hpp"
#include "codegen/MachineModule.hpp"
#include "codegen/MachineOperand.hpp"
#include "codegen/pass/MipsPrinter.hpp"
#include "codegen/pass/MIRPrinter.hpp"
#include "codegen/Register.hpp"
#include "ir/Constants.hpp"
#include "ir/Type.hpp"
#include "util/assert.hpp"
#include "util/Delimeter.hpp"
#include "util/lambda_overload.hpp"

using namespace codegen;

namespace mips_printer {
constexpr char tolower(char c) {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : c;
}

template <std::size_t N>
constexpr auto str_to_lower(const char (&s)[N]) {
    std::array<char, N> out{};
    for (std::size_t i = 0; i < N; ++i) out[i] = tolower(s[i]);
    return out;
}

template <std::size_t N>
static std::ostream& operator<<(std::ostream&       os,
                                std::array<char, N> arrayString) {
    os.write(arrayString.data(), N - 1);
    return os;
}

static std::ostream& operator<<(std::ostream& os, Register reg) {
    os << '$';
    switch (reg.index()) {
#define HANDLE_REG_DEF(name, index) \
    case index:                     \
        os << str_to_lower(#name);  \
        break;
#include "codegen/RegDefs.hpp"
        default:
            os << reg.index();
    }
    return os;
}

static std::ostream& operator<<(std::ostream& os, const MachineOperand& op) {
    std::visit(overloaded{[&os](const RegisterOpKind& reg) { os << reg.reg; },
                          [&os](const ImmediateOpKind& imm) { os << imm.imm; },
                          [&os](const MachineBBOpKind& mbb) {
                              os << mbb.mbb->parent()->getFunction().getName()
                                 << '.' << mbb.mbb->name;
                          },
                          [&os](const GlobalValueOpKind& gv) {
                              ASSERT(gv.offset == 0);
                              os << gv.globalVal->getName();
                          },
                          [&os](const SymbolOpKind& s) {
                              ASSERT(s.offset == 0);
                              os << s.name;
                          }},
               op.getContent());
    return os;
}
}  // namespace mips_printer

using namespace mips_printer;

bool MipsPrinterPass::doInitialization(ir::Module& m) {
    auto& module = getAnalysis<MachineModule>();

    os << ".data\n";

    for (ir::GlobalVariable& gv : m.globals) {
        printGlobalVariable(gv);
        os << '\n';
    }

    os << ".text\n";

    os << "jal main\n"
          "move $a0, $v0\n"
          "li $v0, 17\n"
          "syscall\n\n";

    for (ir::Function& f : m) {
        if (f.isDeclaration()) {
            if (f.getName() == "getint") {
                os << "getint:\n"
                      "    li $v0, 5\n"
                      "    syscall\n"
                      "    jr $ra\n\n";
            } else if (f.getName() == "getchar") {
                os << "getchar:\n"
                      "    li $v0, 12\n"
                      "    syscall\n"
                      "    jr $ra\n\n";
            } else if (f.getName() == "putint") {
                os << "putint:\n"
                      "    li $v0, 1\n"
                      "    syscall\n"
                      "    jr $ra\n\n";
            } else if (f.getName() == "putch") {
                os << "putch:\n"
                      "    li $v0, 11\n"
                      "    syscall\n"
                      "    jr $ra\n\n";
            } else if (f.getName() == "putstr") {
                os << "putstr:\n"
                      "    li $v0, 4\n"
                      "    syscall\n"
                      "    jr $ra\n\n";
            }
            // else if (f.getName() == "putarray") {
            //     os << "putarray:\n"
            //           "    li $v0, 1\n"
            //           "    syscall\n"
            //           "    sll $a0, $a0, 2\n"
            //           "    add $k0, $a0, $a1\n"
            //           "    li $a0, 58\n"
            //           "    li $v0, 11\n"
            //           "    syscall\n"
            //           "    move $k1, $a1\n"
            //           "    putarray.1:\n"
            //           "    li $a0, 32\n"
            //           "    li $v0, 11\n"
            //           "    syscall\n"
            //           "    lw $a0, 0($k1)\n"
            //           "    li $v0, 1\n"
            //           "    syscall\n"
            //           "    addi $k1, $k1, 4\n"
            //           "    bne $k0, $k1, putarray.1\n"
            //           "    li $a0, 10\n"
            //           "    li $v0, 11\n"
            //           "    syscall\n"
            //           "    jr $ra\n\n";
            // }
        } else {
            MachineFunction* mf = module.getMachineFunction(f);
            ASSERT(mf);
            printFunction(*mf);
        }
    }

    return false;
}

void printEscapedString(std::string_view str, std::ostream& os) {
    for (char c : str) {
        switch (c) {
            case '\n':
                os << "\\n";
                break;
            case '\0':
                os << "\\0";
                break;
            default:
                os << c;
        }
    }
}

void MipsPrinterPass::printGlobalVariable(ir::GlobalVariable& gv) {
    os << gv.getName() << ":\n";
    std::string_view indend = "    ";
    os << indend;
    ir::Constant* initializer = gv.getInitializer();
    ASSERT(initializer);
    if (auto* ci = dynamic_cast<ir::ConstantInt*>(initializer)) {
        os << ".word " << ci->getValue();
    } else if (auto* cs = dynamic_cast<ir::ConstantString*>(initializer)) {
        os << ".ascii \"";
        printEscapedString(cs->getAsString(), os);
        os << '"';
    } else if (auto* ca = dynamic_cast<ir::ConstantArray*>(initializer)) {
        Delimeter dlie(", ");
        for (auto& op : ca->operands()) {
            os << ".word ";
            auto* i = dynamic_cast<ir::ConstantInt*>(op.get());
            ASSERT(i);
            os << i->getValue() << '\n' << indend;
        }
    } else if (dynamic_cast<ir::ConstantAggregateZero*>(initializer)) {
        ASSERT(gv.getValueType()->isArrayTy());
        auto* ty = static_cast<ir::ArrayType*>(gv.getValueType());
        os << ".word 0:" << ty->getNumElements();
    }
    os << '\n';
}

void MipsPrinterPass::printFunction(MachineFunction& mf) {
    os << mf.getFunction().getName() << ":\n";
    for (auto& mbb : mf) {
        printBasicBlock(mbb);
    }
    os << std::endl;
}

void MipsPrinterPass::printBasicBlock(MachineBasicBlock& mbb) {
    os << mbb.parent()->getFunction().getName() << '.' << mbb.name << ":\n";
    for (auto& instr : mbb) {
        printInstruction(instr);
    }
}

struct Printer {
    MachineInstr& instr;
    Printer(MachineInstr& instr) : instr(instr) {}
    MachineOperand& getOperand(size_t i) const { return instr.getOperand(i); }
};

struct StoreLoadWord : Printer {};

static std::ostream& operator<<(std::ostream& os, const StoreLoadWord& i) {
    os << i.getOperand(0) << ' ' << i.getOperand(2) << '(' << i.getOperand(1)
       << ')';
    return os;
}
static std::ostream& operator<<(std::ostream& os, const Printer& i) {
    Delimeter deli{", "};
    for (size_t j = 0; j < i.instr.desc.operandsNum; j++) {
        os << deli() << i.getOperand(j);
    }
    return os;
}

void MipsPrinterPass::printInstruction(MachineInstr& instr) {
    os << "    ";
    switch (instr.desc.opcode) {
#define HANDLE_DESC_PRINT(desc_name, print_name, printer) \
    case DESC_##desc_name.opcode:                         \
        os << #print_name " " << printer{instr};          \
        break

        HANDLE_DESC_PRINT(LW, lw, StoreLoadWord);
        HANDLE_DESC_PRINT(SW, sw, StoreLoadWord);

        HANDLE_DESC_PRINT(AND, and, Printer);
        HANDLE_DESC_PRINT(ADD, addu, Printer);
        HANDLE_DESC_PRINT(SUB, subu, Printer);
        HANDLE_DESC_PRINT(MUL, mul, Printer);
        HANDLE_DESC_PRINT(SDIV, div, Printer);
        HANDLE_DESC_PRINT(SREM, rem, Printer);
        HANDLE_DESC_PRINT(XOR, xor, Printer);

        HANDLE_DESC_PRINT(SEQ, seq, Printer);
        HANDLE_DESC_PRINT(SNE, sne, Printer);
        HANDLE_DESC_PRINT(SGT, sgt, Printer);
        HANDLE_DESC_PRINT(SGE, sge, Printer);
        HANDLE_DESC_PRINT(SLT, slt, Printer);
        HANDLE_DESC_PRINT(SLE, sle, Printer);

        HANDLE_DESC_PRINT(ADDI, addiu, Printer);
        HANDLE_DESC_PRINT(ANDI, andi, Printer);
        HANDLE_DESC_PRINT(SLL, sll, Printer);
        HANDLE_DESC_PRINT(SRL, srl, Printer);
        HANDLE_DESC_PRINT(SRA, sra, Printer);
        HANDLE_DESC_PRINT(MULT, mult, Printer);
        HANDLE_DESC_PRINT(MFHI, mfhi, Printer);
        HANDLE_DESC_PRINT(MFLO, mflo, Printer);

        HANDLE_DESC_PRINT(BEQ, beq, Printer);
        HANDLE_DESC_PRINT(BNE, bne, Printer);
        HANDLE_DESC_PRINT(BEQZ, beqz, Printer);
        HANDLE_DESC_PRINT(BNEZ, bnez, Printer);
        HANDLE_DESC_PRINT(BLEZ, blez, Printer);
        HANDLE_DESC_PRINT(BLTZ, bltz, Printer);
        HANDLE_DESC_PRINT(BGEZ, bgez, Printer);
        HANDLE_DESC_PRINT(BGTZ, bgtz, Printer);

        HANDLE_DESC_PRINT(JUMP, j, Printer);
        HANDLE_DESC_PRINT(CALL, jal, Printer);

        HANDLE_DESC_PRINT(LI, li, Printer);
        HANDLE_DESC_PRINT(LA, la, Printer);
        HANDLE_DESC_PRINT(MOVE, move, Printer);
#undef HANDLE_DESC_PRINT

        case DESC_RET.opcode:
            os << "jr $ra";
            break;

        default:
            UNREACHABLE();
    }
    if (!instr.annotation.empty()) {
        os << " # " << instr.annotation;
    }
    os << '\n';
}

void MipsPrinterPass::printOperand(MachineOperand& op) { os << op; }

void MipsPrinterPass::printReg(Register reg, std::ostream& os) { os << reg; }
