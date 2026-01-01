#include "codegen/pass/MIRPrinter.hpp"

#include <ostream>
#include <variant>

#include "codegen/MachineBasicBlock.hpp"
#include "codegen/MachineFunction.hpp"
#include "codegen/MachineModule.hpp"
#include "codegen/MachineOperand.hpp"
#include "codegen/pass/MipsPrinter.hpp"
#include "codegen/Register.hpp"
#include "util/Delimeter.hpp"
#include "util/lambda_overload.hpp"

using namespace codegen;

bool MIRPrinterPass::doInitialization(ir::Module& m) {
    auto& module = getAnalysis<MachineModule>();

    for (auto& f : m) {
        MachineFunction* mf = module.getMachineFunction(f);
        ASSERT(mf);
        printFunction(*mf);
    }

    return false;
}

void MIRPrinterPass::printFunction(MachineFunction& mf) {
    for (auto& [reg, info] : mf.regInfos) {
        os << "# ";
        printReg(reg);
        os << " use count " << info.use_end() - info.use_begin()
           << " def count " << info.def_end() - info.def_begin() << '\n';
    }

    os << mf.getFunction().getName() << ":\n";
    for (auto& mbb : mf) {
        printBasicBlock(mbb);
    }
    os << std::endl;
}

void MIRPrinterPass::printBasicBlock(MachineBasicBlock& mbb) {
    os << mbb.parent()->getFunction().getName() << '.' << mbb.name << ":\n";
    for (auto& instr : mbb) {
        printInstruction(instr);
    }
}

void MIRPrinterPass::printInstruction(MachineInstr& instr) {
    os << "    ";
    Delimeter deli(", ");
    if (instr.desc.explicitDefsNum > 0) {
        for (auto& def : instr.explicit_defs()) {
            os << deli();
            printOperand(def);
        }
        os << " = ";
    }
    os << instr.desc.name << ' ';
    deli.reset();
    for (auto& use : instr.explicit_uses()) {
        os << deli();
        printOperand(use);
    }
    for (auto& other : instr.explicit_others()) {
        os << deli();
        printOperand(other);
    }
    if (!instr.annotation.empty()) os << " # " << instr.annotation;
    os << '\n';
}

void MIRPrinterPass::printOperand(MachineOperand& op) {
    std::visit(
        overloaded{[this](const RegisterOpKind& reg) { printReg(reg.reg); },
                   [this](const ImmediateOpKind& imm) { os << imm.imm; },
                   [this](const MachineBBOpKind& mbb) {
                       os << mbb.mbb->parent()->getFunction().getName() << '.'
                          << mbb.mbb->name;
                   },
                   [this](const GlobalValueOpKind& gv) {
                       os << gv.globalVal->getName() << '(' << gv.offset << ')';
                   },
                   [this](const SymbolOpKind& s) {
                       os << s.name << '(' << s.offset << ')';
                   }},
        op.getContent());
}

void MIRPrinterPass::printReg(Register reg) {
    if (reg.isPhysical()) {
        MipsPrinterPass::printReg(reg, os);
    } else if (reg.isVirtual()) {
        os << "$vir." << reg.index();
    } else if (reg.isStack()) {
        os << "$stc." << reg.index();
    } else {
        UNREACHABLE();
    }
}
