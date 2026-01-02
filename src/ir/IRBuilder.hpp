#pragma once

#include <cstdint>
#include <utility>
#include <vector>

#include "ir/BasicBlock.hpp"
#include "ir/Constants.hpp"
#include "ir/Function.hpp"
#include "ir/GlobalValue.hpp"
#include "ir/Instructions.hpp"
#include "ir/LLVMContext.hpp"
#include "ir/Module.hpp"
#include "ir/Type.hpp"
#include "util/IntrusiveList.hpp"

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
    PointerType* getPtrTy(unsigned addrSpace = 0) {
        return PointerType::get(context, addrSpace);
    }

    // Constants
    GlobalVariable* CreateGlobalString(std::string str, std::string name,
                                       bool addNull = true) {
        if (addNull) str.push_back('\0');
        Module* module   = bb->parent()->parent();
        auto*   constStr = ConstantString::getString(context, std::move(str));
        auto*   gv       = new GlobalVariable{constStr->getType(), true,
                                      GlobalValue::InternalLinkage, constStr,
                                      std::move(name)};
        module->globals.push_back(gv);
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
        auto* inst = new ReturnInst{context, nullptr};
        bb->push_back(inst);
        return inst;
    }
    ReturnInst* CreateRet(Value* v) {
        auto* inst = new ReturnInst{context, v};
        bb->push_back(inst);
        return inst;
    }
    BranchInst* CreateBr(BasicBlock* dst) {
        auto* inst = new BranchInst{dst};
        bb->push_back(inst);
        return inst;
    }
    BranchInst* CreateCondBr(Value* cond, BasicBlock* ifTrue,
                             BasicBlock* ifFalse) {
        auto* inst = new BranchInst{ifTrue, ifFalse, cond};
        bb->push_back(inst);
        return inst;
    }
    CallInst* CreateCall(FunctionType* fTy, Value* callee,
                         const std::vector<Value*>& args, std::string name) {
        auto* inst = new CallInst{fTy, callee, args, std::move(name)};
        bb->push_back(inst);
        return inst;
    }

    // Instruction - BinaryOp
   public:
    BinaryOperator* CreateNSWAdd(Value* lhs, Value* rhs, std::string name) {
        auto* inst = new BinaryOperator{BinaryOperator::Add, lhs, rhs,
                                        lhs->getType(), std::move(name)};
        bb->push_back(inst);
        return inst;
    }
    BinaryOperator* CreateNSWSub(Value* lhs, Value* rhs, std::string name) {
        auto* inst = new BinaryOperator{BinaryOperator::Sub, lhs, rhs,
                                        lhs->getType(), std::move(name)};
        bb->push_back(inst);
        return inst;
    }
    BinaryOperator* CreateNSWMul(Value* lhs, Value* rhs, std::string name) {
        auto* inst = new BinaryOperator{BinaryOperator::Mul, lhs, rhs,
                                        lhs->getType(), std::move(name)};
        bb->push_back(inst);
        return inst;
    }
    BinaryOperator* CreateSDiv(Value* lhs, Value* rhs, std::string name) {
        auto* inst = new BinaryOperator{BinaryOperator::SDiv, lhs, rhs,
                                        lhs->getType(), std::move(name)};
        bb->push_back(inst);
        return inst;
    }
    BinaryOperator* CreateSRem(Value* lhs, Value* rhs, std::string name) {
        auto* inst = new BinaryOperator{BinaryOperator::SRem, lhs, rhs,
                                        lhs->getType(), std::move(name)};
        bb->push_back(inst);
        return inst;
    }
    BinaryOperator* CreateXor(Value* lhs, Value* rhs, std::string name) {
        auto* inst = new BinaryOperator{BinaryOperator::Xor, lhs, rhs,
                                        lhs->getType(), std::move(name)};
        bb->push_back(inst);
        return inst;
    }
    BinaryOperator* CreateNSWNeg(Value* v, std::string name) {
        return CreateNSWSub(
            ConstantInt::get(static_cast<IntegerType*>(v->getType()), 0), v,
            std::move(name));
    }

    // Instruction - Compare
    ICmpInst* CreateICmp(CmpInst::Predicate p, Value* lhs, Value* rhs,
                         std::string name) {
        auto* inst = new ICmpInst{p, lhs, rhs, std::move(name)};
        bb->push_back(inst);
        return inst;
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
        switch (op) {
            case CastInst::SExt:
                return CreateSExt(v, destTy, std::move(name));
            case CastInst::ZExt:
                return CreateZExt(v, destTy, std::move(name));
        }
    }
    SExtInst* CreateSExt(Value* v, Type* destTy, std::string name) {
        auto* inst = new SExtInst{v, destTy, std::move(name)};
        bb->push_back(inst);
        return inst;
    }
    ZExtInst* CreateZExt(Value* v, Type* destTy, std::string name) {
        auto* inst = new ZExtInst{v, destTy, std::move(name)};
        bb->push_back(inst);
        return inst;
    }

    // Instruction - Memory
    AllocaInst* CreateEntryBlockAlloca(Type* ty, Value* arraySize,
                                       std::string name) {
        if (arraySize == nullptr) {
            arraySize = ConstantInt::get(getInt32Ty(), 1);
        }
        auto* inst = new AllocaInst{ty, 0, arraySize, std::move(name)};
        bb->parent()->front().push_front(inst);
        return inst;
    }
    AllocaInst* CreateAlloca(Type* ty, Value* arraySize, std::string name) {
        if (arraySize == nullptr) {
            arraySize = ConstantInt::get(getInt32Ty(), 1);
        }
        auto* inst = new AllocaInst{ty, 0, arraySize, std::move(name)};
        bb->push_back(inst);
        return inst;
    }
    LoadInst* CreateLoad(Type* ty, Value* ptr, std::string name) {
        auto* inst = new LoadInst{ty, ptr, std::move(name)};
        bb->push_back(inst);
        return inst;
    }
    StoreInst* CreateStore(Value* val, Value* ptr) {
        auto* inst = new StoreInst{val, ptr};
        bb->push_back(inst);
        return inst;
    }
    GetElementPtrInst* CreateGEP(Type* pointeeTy, Value* ptr,
                                 const std::vector<Value*> idxList,
                                 std::string               name) {
        auto* inst =
            new GetElementPtrInst{pointeeTy, ptr, idxList, std::move((name))};
        bb->push_back(inst);
        return inst;
    }
};
}  // namespace ir
