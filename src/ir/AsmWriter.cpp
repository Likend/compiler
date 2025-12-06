#include <iomanip>
#include <ios>
#include <ostream>
#include <string_view>

#include "ir/BasicBlock.hpp"
#include "ir/Constants.hpp"
#include "ir/Function.hpp"
#include "ir/GlobalValue.hpp"
#include "ir/Instructions.hpp"
#include "ir/Module.hpp"
#include "ir/Type.hpp"
#include "ir/Value.hpp"

using namespace ir;

enum PrefixType {
    GlobalPrefix,
    ComdatPrefix,
    LabelPrefix,
    LocalPrefix,
    NoPrefix
};

static std::ostream& operator<<(std::ostream& os, PrefixType type) {
    switch (type) {
        case LabelPrefix:
        case NoPrefix:
            break;
        case GlobalPrefix:
            os << '@';
            break;
        case ComdatPrefix:
            os << '$';
            break;
        case LocalPrefix:
            os << '%';
            break;
        default:
            UNREACHABLE();
    }
    return os;
}

std::string_view predicateString(CmpInst::Predicate type) {
    switch (type) {
        case CmpInst::ICMP_EQ:
            return "eq";
        case CmpInst::ICMP_NE:
            return "ne";
        case CmpInst::ICMP_SGT:
            return "sgt";
        case CmpInst::ICMP_SGE:
            return "sge";
        case CmpInst::ICMP_SLT:
            return "slt";
        case CmpInst::ICMP_SLE:
            return "sle";
        default:
            UNREACHABLE();
    }
}

static std::ostream& operator<<(std::ostream& os, CmpInst::Predicate type) {
    os << predicateString(type);
    return os;
}

std::string_view linkageString(GlobalValue::LinkageTypes linkage) {
    switch (linkage) {
        case GlobalValue::ExternalLinkage:
            return "";
        case GlobalValue::InternalLinkage:
            return "internal";
        default:
            UNREACHABLE();
    }
}

static std::ostream& operator<<(std::ostream&             os,
                                GlobalValue::LinkageTypes type) {
    os << linkageString(type);
    return os;
}

static PrefixType getNamePrefix(const Value* val) {
    if (dynamic_cast<const GlobalValue*>(val))
        return GlobalPrefix;
    else
        return LocalPrefix;
}

class Delimeter {
    std::string_view deli;
    bool             isFirstTime = true;

   public:
    Delimeter(std::string_view deli) : deli(deli) {}

    std::string_view operator()() {
        if (isFirstTime) {
            isFirstTime = false;
            return "";
        } else
            return deli;
    }
};

class Printer {
    std::ostream& os;

   public:
    Printer(std::ostream& os) : os(os) {}

    void printValueName(const Value* value) {
        os << getNamePrefix(value) << value->getName();
    }

    void printArgument(const Argument* arg) {
        os << arg->getType();
        if (!arg->getName().empty()) {
            os << ' ';
            printValueName(arg);
        }
    }

    void printBaseBlock(const BasicBlock* bb) {
        os << '\n';
        os << bb->getName() << ":\n";
        // print pred comment
        for (const auto& instruction : *bb) {
            printInstruction(&instruction);
            os << '\n';
        }
    }

    void printOperand(const Value* operand, bool printType) {
        if (!operand) {
            os << "<null operand!>";
            return;
        }
        if (printType) {
            os << operand->getType() << ' ';
        }

        printOperandInternal(operand);
    }

    void printEscapedString(std::string_view str) {
        for (char c : str) {
            switch (c) {
                case '\n':
                case '\0':
                    os << '\\';
                    os << std::hex << std::setw(2) << std::setfill('0')
                       << static_cast<uint32_t>(c) << std::dec;
                    break;
                default:
                    os << c;
                    break;
            }
        }
    }

    void printConstant(const Constant* constant) {
        if (const auto* ci = dynamic_cast<const ConstantInt*>(constant)) {
            if (ci->getBitWidth() == 1) {
                os << (ci->getValue() ? "true" : "false");
            } else {
                os << ci->getValue();
            }
        } else if (const auto* ca =
                       dynamic_cast<const ConstantArray*>(constant)) {
            Type* ty = ca->getType()->getElementType();
            os << '[';
            Delimeter deli(", ");
            for (const auto& op : ca->operands()) {
                os << deli() << ty << ' ';
                printOperandInternal(op.get());
            }
            os << ']';
        } else if (const auto* ca =
                       dynamic_cast<const ConstantString*>(constant)) {
            os << "c\"";
            printEscapedString(ca->getAsString());
            os << '"';
        }
    }

    void printOperandInternal(const Value* operand) {
        if (const auto* constant = dynamic_cast<const Constant*>(operand);
            constant && dynamic_cast<const GlobalValue*>(constant) == nullptr) {
            printConstant(constant);
        } else {
            printValueName(operand);
        }
    }

    void printInstruction(const Instruction* inst) {
        // indent
        os << "  ";
        if (!inst->getType()->isVoidTy()) {
            printValueName(inst);
            os << " = ";
        }
        os << inst->getOpcodeName();

        if (const auto* i = dynamic_cast<const BranchInst*>(inst)) {
            os << ' ';
            if (i->isConditional()) {
                printOperand(i->getCondition(), true);
                os << ", ";
                printOperand(i->getTrueBB(), true);
                os << ", ";
                printOperand(i->getFalseBB(), true);
            } else {
                printOperand(i->getTrueBB(), true);
            }
        } else if (const auto* i = dynamic_cast<const ReturnInst*>(inst)) {
            if (auto* value = i->getReturnValue()) {
                os << ' ';
                printOperand(value, true);
            } else {
                os << " void";
            }
        } else if (const auto* i = dynamic_cast<const CallInst*>(inst)) {
            const FunctionType* fTy = i->getFunctionType();
            os << ' ' << fTy->getReturnType() << ' ';
            printOperand(i->getCalledOperand(), false);
            os << '(';
            Delimeter deli(", ");
            for (const auto& arg : i->args()) {
                os << deli();
                printOperand(arg.get(), true);
            }
            os << ')';
        } else if (const auto* i = dynamic_cast<const AllocaInst*>(inst)) {
            os << ' ' << i->getAllocatedType();
        } else if (const auto* i = dynamic_cast<const LoadInst*>(inst)) {
            os << ' ' << i->getType() << ", ";
            printOperand(i->getPointerOperand(), true);
        } else if (const auto* i = dynamic_cast<const StoreInst*>(inst)) {
            os << ' ';
            printOperand(i->getValueOperand(), true);
            os << ", ";
            printOperand(i->getPointerOperand(), true);
        } else if (const auto* i =
                       dynamic_cast<const GetElementPtrInst*>(inst)) {
            os << ' ' << i->getSourceElementType();
            for (const auto& op : i->operands()) {
                os << ", ";
                printOperand(op.get(), true);
            }
        } else if (const auto* i = dynamic_cast<const ICmpInst*>(inst)) {
            os << ' ' << i->getPredicate() << ' ' << i->getLHS()->getType()
               << ' ';
            printOperand(i->getLHS(), false);
            os << ", ";
            printOperand(i->getRHS(), false);
        } else if (const auto* i = dynamic_cast<const CastInst*>(inst)) {
            os << ' ';
            printOperand(i->getSrc(), true);
            os << " to " << i->getDestTy();
        } else if (const auto* i = dynamic_cast<const BinaryOperator*>(inst)) {
            os << ' ' << i->getLHS()->getType() << ' ';
            printOperand(i->getLHS(), false);
            os << ", ";
            printOperand(i->getRHS(), false);
        } else {
            UNREACHABLE();
        }
    }

    void printFunction(const Function* f) {
        if (f->isDeclaration()) {
            os << "declare ";
        } else {
            os << "define ";
        }
        os << f->getLinkageType() << ' ';
        os << f->getReturnType() << ' ';
        printValueName(f);

        os << '(';
        Delimeter deli(", ");
        for (const Argument& arg : f->args()) {
            os << deli();
            printArgument(&arg);
        }
        os << ") ";

        if (f->isDeclaration()) {
            os << '\n';
        } else {
            os << '{';

            for (const BasicBlock& bb : *f) {
                printBaseBlock(&bb);
            }
            // printuselist?

            os << "}\n";
        }
    }

    void printModule(const Module* module) {
        // os << "; ModuleID = '" << module->getName() << "'\n";
        for (const GlobalVariable& gv : module->globals()) {
            printGlobal(&gv);
            os << '\n';
        }
        for (const Function& func : module->functions()) {
            printFunction(&func);
            os << '\n';
        }
    }

    void printGlobal(const GlobalVariable* gv) {
        printOperandInternal(gv);
        os << " = ";

        os << (gv->isConstant() ? "constant " : "global ");
        os << gv->getValueType();
        if (gv->hasInitializer()) {
            os << ' ';
            printOperand(gv->getInitializer(), false);
        }
    }
};

// print module
std::ostream& ir::operator<<(std::ostream& os, const Module* module) {
    Printer printer{os};
    printer.printModule(module);
    return os;
}

// print function
std::ostream& ir::operator<<(std::ostream& os, const Function* function) {
    Printer printer{os};
    printer.printFunction(function);
    return os;
}

// print type
std::ostream& ir::operator<<(std::ostream& os, const Type* type) {
    switch (type->getTypeID()) {
        case Type::VoidTyID:
            os << "void";
            break;
        case Type::LabelTyID:
            os << "label";
            break;
        case Type::IntegerTyID:
            os << 'i' << static_cast<const IntegerType*>(type)->getBitWidth();
            break;
        case Type::FunctionTyID: {
            const auto* fTy = static_cast<const FunctionType*>(type);
            os << fTy->getReturnType() << '(';
            Delimeter deli(", ");
            for (Type* ty : fTy->params()) {
                os << deli();
                os << ty;
            }
            break;
        }
        case Type::PointerTyID:
            os << static_cast<const PointerType*>(type)->getPointeeType()
               << '*';
            break;
        case Type::ArrayTyID: {
            const auto* aTy = static_cast<const ArrayType*>(type);
            os << '[' << aTy->getNumElements() << " x " << aTy->getElementType()
               << ']';
            break;
        }
        default:
            UNREACHABLE();
    }
    return os;
}
