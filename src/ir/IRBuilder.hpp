#pragma once

#include <cstdint>
#include <vector>

#include "ir/BasicBlock.hpp"
#include "ir/Constants.hpp"
#include "ir/Function.hpp"
#include "ir/GlobalValue.hpp"
#include "ir/Instructions.hpp"
#include "ir/LLVMContext.hpp"
#include "ir/Type.hpp"

namespace ir {

class IRBuilder {
    BasicBlock*  bb = nullptr;
    LLVMContext& context;

   public:
    IRBuilder(LLVMContext& context) : context(context) {};

    BasicBlock* GetInsertBlock() const { return bb; }
    // todo insert point
    LLVMContext& getContext() const { return context; }

    void SetInsertPoint(BasicBlock* theBB) { bb = theBB; }
    // todo set insert point use iterator

    // Types
    IntegerType* getInt1Ty() { return IntegerType::get(context, 1); }
    IntegerType* getInt8Ty() { return IntegerType::get(context, 8); }
    IntegerType* getInt32Ty() { return IntegerType::get(context, 32); }
    Type*        getVoidTy() { return Type::getVoidTy(context); }
    PointerType* getPtrTy(Type* pointToTy, unsigned addrSpace = 0) {
        return PointerType::get(pointToTy, addrSpace);
    }

    // Constants
    GlobalVariable* CreateGlobalString(std::string str, std::string name,
                                       bool addNull = true) {
        if (addNull) str.push_back('\0');
        Module* module   = bb->getParent()->getParent();
        auto*   constStr = ConstantString::getString(context, std::move(str));
        auto*   gv = GlobalVariable::Create(*module, constStr->getType(), true,
                                            GlobalValue::InternalLinkage,
                                            constStr, std::move(name));
        return gv;
    }
    ConstantInt* getInt1(bool v) {
        return ConstantInt::get(getInt1Ty(), static_cast<int32_t>(v));
    };
    ConstantInt* getInt32(uint32_t v) {
        return ConstantInt::get(getInt32Ty(), static_cast<int32_t>(v));
    }

    // Instructions
    ReturnInst* CreateRetVoid() {
        return ReturnInst::Create(context, nullptr, bb);
    }
    ReturnInst* CreateRet(Value* v) {
        return ReturnInst::Create(context, v, bb);
    }
    BranchInst* CreateBr(BasicBlock* dst) {
        return BranchInst::Create(dst, bb);
    }
    BranchInst* CreateCondBr(Value* cond, BasicBlock* ifTrue,
                             BasicBlock* ifFalse) {
        return BranchInst::Create(ifTrue, ifFalse, cond, bb);
    }
    CallInst* CreateCall(FunctionType* fTy, Value* callee,
                         const std::vector<Value*>& args, std::string name) {
        return CallInst::Create(fTy, callee, args, std::move(name), bb);
    }

    // Instruction - BinaryOp
   public:
    Value* CreateNSWAdd(Value* lhs, Value* rhs, std::string name) {
        return BinaryOperator::Create(BinaryOperator::Add, lhs, rhs,
                                      std::move(name), bb);
    }
    Value* CreateNSWSub(Value* lhs, Value* rhs, std::string name) {
        return BinaryOperator::Create(BinaryOperator::Sub, lhs, rhs,
                                      std::move(name), bb);
    }
    Value* CreateNSWMul(Value* lhs, Value* rhs, std::string name) {
        return BinaryOperator::Create(BinaryOperator::Mul, lhs, rhs,
                                      std::move(name), bb);
    }
    Value* CreateSDiv(Value* lhs, Value* rhs, std::string name) {
        return BinaryOperator::Create(BinaryOperator::SDiv, lhs, rhs,
                                      std::move(name), bb);
    }
    Value* CreateSRem(Value* lhs, Value* rhs, std::string name) {
        return BinaryOperator::Create(BinaryOperator::SRem, lhs, rhs,
                                      std::move(name), bb);
    }
    Value* CreateXor(Value* lhs, Value* rhs, std::string name) {
        return BinaryOperator::Create(BinaryOperator::Xor, lhs, rhs,
                                      std::move(name), bb);
    }
    Value* CreateNSWNeg(Value* v, std::string name) {
        return CreateNSWSub(
            ConstantInt::get(static_cast<IntegerType*>(v->getType()), 0), v,
            std::move(name));
    }

    // Instruction - Compare
    ICmpInst* CreateICmp(CmpInst::Predicate p, Value* lhs, Value* rhs,
                         std::string name) {
        return ICmpInst::Create(p, lhs, rhs, std::move(name), bb);
    }
    ICmpInst* CreateICmpEQ(Value* lhs, Value* rhs, std::string name) {
        return CreateICmp(CmpInst::ICMP_EQ, lhs, rhs, std::move(name));
    }
    ICmpInst* CreateICmpNE(Value* lhs, Value* rhs, std::string name) {
        return CreateICmp(CmpInst::ICMP_NE, lhs, rhs, std::move(name));
    }
    ICmpInst* CreateICmpSLT(Value* lhs, Value* rhs, std::string name) {
        return CreateICmp(CmpInst::ICMP_SLT, lhs, rhs, std::move(name));
    }
    ICmpInst* CreateICmpSLE(Value* lhs, Value* rhs, std::string name) {
        return CreateICmp(CmpInst::ICMP_SLE, lhs, rhs, std::move(name));
    }
    ICmpInst* CreateICmpSGT(Value* lhs, Value* rhs, std::string name) {
        return CreateICmp(CmpInst::ICMP_SGT, lhs, rhs, std::move(name));
    }
    ICmpInst* CreateICmpSGE(Value* lhs, Value* rhs, std::string name) {
        return CreateICmp(CmpInst::ICMP_SGE, lhs, rhs, std::move(name));
    }

    // Instruction - Cast/Conversion
    Value* CreateCast(CastInst::CastOps op, Value* v, Type* destTy,
                      std::string name) {
        if (v->getType() == destTy) return v;
        return CastInst::Create(op, v, destTy, std::move(name), bb);
    }
    Value* CreateSExt(Value* v, Type* destTy, std::string name) {
        return CreateCast(CastInst::SExt, v, destTy, std::move(name));
    }

    // Instruction - Memory
    AllocaInst* CreateAlloca(Type* ty, Value* arraySize, std::string name) {
        if (arraySize == nullptr) {
            arraySize = ConstantInt::get(getInt32Ty(), 1);
        }
        return AllocaInst::Create(ty, 0, arraySize, std::move(name), bb);
    }
    LoadInst* CreateLoad(Type* ty, Value* ptr, std::string name) {
        return LoadInst::Create(ty, ptr, std::move(name), bb);
    }
    StoreInst* CreateStore(Value* val, Value* ptr) {
        return StoreInst::Create(val, ptr, bb);
    }
    GetElementPtrInst* CreateGEP(Value* ptr, const std::vector<Value*> idxList,
                                 std::string name) {
        return GetElementPtrInst::Create(ptr, idxList, std::move(name), bb);
    }
};
}  // namespace ir
