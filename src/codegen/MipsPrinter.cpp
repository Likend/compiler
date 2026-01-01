#include "codegen/MipsPrinter.hpp"

#include <ostream>
#include <variant>

#include "codegen/MachineBasicBlock.hpp"
#include "codegen/MachineFunction.hpp"
#include "codegen/MachineInstr.hpp"
#include "codegen/MachineModule.hpp"
#include "codegen/MachineOperand.hpp"
#include "codegen/MipsPrinter.hpp"
#include "codegen/MIRPrinter.hpp"
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
                      "    lw $a0, 0($sp)\n"
                      "    li $v0, 1\n"
                      "    syscall\n"
                      "    jr $ra\n\n";
            } else if (f.getName() == "putch") {
                os << "putch:\n"
                      "    lw $a0, 0($sp)\n"
                      "    li $v0, 11\n"
                      "    syscall\n"
                      "    jr $ra\n\n";
            } else if (f.getName() == "putstr") {
                os << "putstr:\n"
                      "    lw $a0, 0($sp)\n"
                      "    li $v0, 4\n"
                      "    syscall\n"
                      "    jr $ra\n\n";
            } else if (f.getName() == "putarray") {
                os << "putarray:\n"
                      "    lw $a0, 0($sp)\n"
                      "    lw $a1, 4($sp)\n"
                      "    li $v0, 1\n"
                      "    syscall\n"
                      "    sll $a0, $a0, 2\n"
                      "    add $k0, $a0, $a1\n"
                      "    li $a0, 58\n"
                      "    li $v0, 11\n"
                      "    syscall\n"
                      "    move $k1, $a1\n"
                      "    putarray.1:\n"
                      "    li $a0, 32\n"
                      "    li $v0, 11\n"
                      "    syscall\n"
                      "    lw $a0, 0($k1)\n"
                      "    li $v0, 1\n"
                      "    syscall\n"
                      "    addi $k1, $k1, 4\n"
                      "    bne $k0, $k1, putarray.1\n"
                      "    li $a0, 10\n"
                      "    li $v0, 11\n"
                      "    syscall\n"
                      "    jr $ra\n\n";
            }
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
struct Binary : Printer {};

static std::ostream& operator<<(std::ostream& os, const StoreLoadWord& i) {
    os << i.getOperand(0) << ' ' << i.getOperand(2) << '(' << i.getOperand(1)
       << ')';
    return os;
}
static std::ostream& operator<<(std::ostream& os, const Binary& i) {
    os << i.getOperand(0) << ", " << i.getOperand(1) << ", " << i.getOperand(2);
    return os;
}

void MipsPrinterPass::printInstruction(MachineInstr& instr) {
    os << "    ";
    switch (instr.desc.opcode) {
        case DESC_LW.opcode:
            os << "lw " << StoreLoadWord{instr};
            break;
        case DESC_SW.opcode:
            os << "sw " << StoreLoadWord{instr};
            break;

        case DESC_ADD.opcode:
            os << "addu " << Binary{instr};
            break;
        case DESC_SUB.opcode:
            os << "subu " << Binary{instr};
            break;
        case DESC_MUL.opcode:
            os << "mul " << Binary{instr};
            break;
        case DESC_SDIV.opcode:
            os << "div " << Binary{instr};
            break;
        case DESC_SREM.opcode:
            os << "rem " << Binary{instr};
            break;
        case DESC_XOR.opcode:
            os << "xor " << Binary{instr};
            break;

        case DESC_SEQ.opcode:
            os << "seq " << Binary{instr};
            break;
        case DESC_SNE.opcode:
            os << "sne " << Binary{instr};
            break;
        case DESC_SGT.opcode:
            os << "sgt " << Binary{instr};
            break;
        case DESC_SGE.opcode:
            os << "sge " << Binary{instr};
            break;
        case DESC_SLT.opcode:
            os << "slt " << Binary{instr};
            break;
        case DESC_SLE.opcode:
            os << "sle " << Binary{instr};
            break;

        case DESC_ADDI.opcode:
            os << "addiu " << Binary{instr};
            break;
        case DESC_MULTI.opcode:
            os << "mul " << Binary{instr};
            break;
        case DESC_SLL.opcode:
            os << "sll " << Binary{instr};
            break;
        case DESC_SRL.opcode:
            os << "srl" << Binary{instr};
            break;

        case DESC_RET.opcode:
            os << "jr $ra";
            break;
        case DESC_BEQZ.opcode:
            os << "beqz " << instr.getOperand(0) << ", " << instr.getOperand(1);
            break;
        case DESC_JUMP.opcode:
            os << "j " << instr.getOperand(0);
            break;
        case DESC_CALL.opcode:
            os << "jal " << instr.getOperand(0);
            break;

        case DESC_LI.opcode:
            os << "li " << instr.getOperand(0) << ", " << instr.getOperand(1);
            break;
        case DESC_LA.opcode:
            os << "la " << instr.getOperand(0) << ", " << instr.getOperand(1);
            break;
        case DESC_MOVE.opcode:
            os << "move " << instr.getOperand(0) << ", " << instr.getOperand(1);
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
