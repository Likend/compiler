#include "ir/Instructions.hpp"

#include <memory>

#include "ir/BasicBlock.hpp"
#include "ir/Function.hpp"
#include "ir/Type.hpp"

using namespace ir;

Instruction::Instruction(Type* ty, size_t numOperands, BasicBlock* parent)
    : User(ty, numOperands), parent(parent) {
    ASSERT_WITH(parent, "Cannot add to null parent currently");
    parent->instList.emplace_back(std::unique_ptr<Instruction>(this));
}

GetElementPtrInst::GetElementPtrInst(Type* pointeeTy, Value* ptr,
                                     const std::vector<Value*>& idxList,
                                     std::string name, BasicBlock* parent)
    : Instruction(ptr->getType(),  // PointerType
                  idxList.size() + 1, parent),
      sourceElementType(pointeeTy),
      resultElementType(getIndexedType(pointeeTy, idxList)) {
    ASSERT(ptr->getType()->isPointerTy());
    setOperand(0, ptr);
    for (size_t i = 0; i < idxList.size(); i++) {
        setOperand(i + 1, idxList[i]);
    }
    setName(std::move(name));
}

Type* GetElementPtrInst::getTypeAtIndex(Type* type, Value* idx) {
    if (!idx->getType()->isIntegerTy()) return nullptr;
    if (type->isArrayTy()) {
        auto* arrTy = static_cast<ArrayType*>(type);
        return arrTy->getElementType();
    }
    return nullptr;
}

Type* GetElementPtrInst::getIndexedType(Type*                      pointeeType,
                                        const std::vector<Value*>& idxList) {
    if (idxList.empty()) return pointeeType;
    for (auto it = idxList.begin() + 1; it != idxList.end(); ++it) {
        Value* v    = *it;
        pointeeType = getTypeAtIndex(pointeeType, v);
        if (pointeeType == nullptr) return pointeeType;
    }
    return pointeeType;
}

CallInst::CallInst(FunctionType* ty, Value* func,
                   const std::vector<Value*>& args, std::string name,
                   BasicBlock* parent)
    : Instruction(ty->getReturnType(), args.size() + 1, parent), funcTy(ty) {
    setOperand(getNumOperands() - 1, func);
    for (size_t i = 0; i < args.size(); i++) {
        setOperand(i, args[i]);
    }
    setName(std::move(name));
}

BranchInst::BranchInst(BasicBlock* ifTrue, BasicBlock* ifFalse, Value* cond,
                       BasicBlock* parent)
    : Instruction(Type::getVoidTy(ifTrue->getContext()), 3, parent) {
    setOperand(2, ifTrue);   // numOperands -1
    setOperand(1, ifFalse);  // numOperands -2
    setOperand(0, cond);     // numOperands -3
}

BranchInst::BranchInst(BasicBlock* ifTrue, BasicBlock* parent)
    : Instruction(Type::getVoidTy(ifTrue->getContext()), 1, parent) {
    setOperand(0, ifTrue);  // numOperands -1
}

BasicBlock* BranchInst::getTrueBB() const {
    return dynamic_cast<BasicBlock*>(getOperand(getNumOperands() - 1));
}

BasicBlock* BranchInst::getFalseBB() const {
    if (isConditional())
        return dynamic_cast<BasicBlock*>(getOperand(1));
    else
        return nullptr;
}

CastInst* CastInst::Create(CastOps ops, Value* s, Type* ty, std::string name,
                           BasicBlock* parent) {
    switch (ops) {
        case SExt:
            return SExtInst::Create(s, ty, std::move(name), parent);
        case ZExt:
            return ZExtInst::Create(s, ty, std::move(name), parent);
        default:
            UNREACHABLE();
    }
}
