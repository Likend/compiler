#include "codegen/MachineOperand.hpp"

// #include <variant>
#include "codegen/MachineInstr.hpp"
// #include "util/lambda_overload.hpp"

using namespace codegen;

bool MachineOperand::isUse() const { return getParent().operandIsUse(*this); }
bool MachineOperand::isDef() const { return getParent().operandIsDef(*this); }

size_t MachineOperand::getOperandNo() const {
    return getParent().getOperandNo(this);
}

// void MachineOperand::ChangeTo(MachineOperandContent newContent) {
//     std::visit(overloaded{
//         [](const RegisterOpKind& reg) {
//             if()
//         }
//     }, newContent);
// }
