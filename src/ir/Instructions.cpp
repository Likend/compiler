#include "ir/Instructions.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ir/BasicBlock.hpp"
#include "ir/LLVMContext.hpp"
#include "ir/Type.hpp"
#include "ir/ValueSymbolTable.hpp"
#include "util/assert.hpp"

using namespace ir;

GetElementPtrInst::GetElementPtrInst(Type* pointeeTy, Value* ptr,
                                     const std::vector<Value*>& idxList,
                                     std::string                name)
    : Instruction(ptr->getType(),  // PointerType
                  idxList.size() + 1),
      sourceElementType(pointeeTy),
      resultElementType(getIndexedType(pointeeTy, idxList)) {
    ASSERT(ptr->getType()->isPointerTy());
    setOperand(0, ptr);
    for (size_t i = 0; i < idxList.size(); i++) {
        setOperand(i + 1, idxList[i]);
    }
    setName(std::move(name));
}

GetElementPtrInst* GetElementPtrInst::clone(
    const std::function<Value*(const Value*)>& vmap_lookup) const {
    std::vector<Value*> idxList;
    idxList.reserve(idx_end() - idx_begin());
    std::transform(idx_begin(), idx_end(), std::back_inserter(idxList),
                   vmap_lookup);
    return new GetElementPtrInst{getSourceElementType(),
                                 vmap_lookup(getPointerOperand()), idxList,
                                 std::string(getName())};
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
                   const std::vector<Value*>& args, std::string name)
    : Instruction(ty->getReturnType(), args.size() + 1), funcTy(ty) {
    setOperand(getNumOperands() - 1, func);
    for (size_t i = 0; i < args.size(); i++) {
        setOperand(i, args[i]);
    }
    setName(std::move(name));
}

CallInst* CallInst::clone(
    const std::function<Value*(const Value*)>& vmap_lookup) const {
    std::vector<Value*> args;
    args.reserve(arg_size());
    std::transform(arg_begin(), arg_end(), std::back_inserter(args),
                   vmap_lookup);
    return new CallInst{getFunctionType(), vmap_lookup(getCalledOperand()),
                        args, std::string(getName())};
}

BranchInst::BranchInst(BasicBlock* ifTrue, BasicBlock* ifFalse, Value* cond)
    : TerminatorInst(Type::getVoidTy(ifTrue->getContext()), 3) {
    setOperand(0, ifTrue);   // numOperands -1
    setOperand(1, ifFalse);  // numOperands -2
    setOperand(2, cond);     // numOperands -3
}

BranchInst::BranchInst(BasicBlock* ifTrue)
    : TerminatorInst(Type::getVoidTy(ifTrue->getContext()), 1) {
    setOperand(0, ifTrue);  // numOperands -1
}

BasicBlock* BranchInst::getTrueBB() const {
    return dynamic_cast<BasicBlock*>(getOperand(0));
}

BasicBlock* BranchInst::getFalseBB() const {
    if (isConditional())
        return dynamic_cast<BasicBlock*>(getOperand(1));
    else
        return nullptr;
}

BasicBlock* BranchInst::getSuccessor(unsigned i) const {
    ASSERT_WITH(i < getNumSuccessors(), "Successor # out of range for Branch!");
    return dynamic_cast<BasicBlock*>(getOperand(i));
}

BranchInst* BranchInst::clone(
    const std::function<Value*(const Value*)>& vmap_lookup) const {
    if (isConditional())
        return new BranchInst{
            dynamic_cast<BasicBlock*>(vmap_lookup(getTrueBB())),
            dynamic_cast<BasicBlock*>(vmap_lookup(getFalseBB())),
            vmap_lookup(getCondition())};
    else
        return new BranchInst{
            dynamic_cast<BasicBlock*>(vmap_lookup(getTrueBB())),
        };
}

bool PHINode::isComplete() const {
    return std::all_of(parent()->pred_begin(), parent()->pred_end(),
                       [this](const BasicBlock& pred) {
                           return getBasicBlockIndex(&pred) !=
                                  static_cast<size_t>(-1);
                       });
}

PHINode* PHINode::clone(
    const std::function<Value*(const Value*)>& vmap_lookup) const {
    auto* newPhi =
        new PHINode{getType(), ReservedSpace, std::string(getName())};
    for (size_t i = 0; i < getNumIncomingValues(); ++i) {
        newPhi->setIncomingBlock(
            i, dynamic_cast<BasicBlock*>(vmap_lookup(getIncomingBlock(i))));
        newPhi->setIncomingValue(i, vmap_lookup(getIncomingValue(i)));
    }
    return newPhi;
}
