#pragma once

#include <string>
#include <string_view>
#include <utility>

#include "ir/LLVMContext.hpp"
#include "ir/Type.hpp"
#include "ir/User.hpp"
#include "util/assert.hpp"

namespace ir {
class BasicBlock;

class Instruction : public User {
   protected:
    Instruction(Type* ty, size_t numOperands, BasicBlock* parent);

    BasicBlock* parent;

   public:
    Instruction(const Instruction&)            = delete;
    Instruction& operator=(const Instruction&) = delete;

    BasicBlock* getParent() const { return parent; }

    virtual std::string_view getOpcodeName() const = 0;
};

class UnaryInstruction : public Instruction {
   protected:
    UnaryInstruction(Type* ty, Value* v, BasicBlock* parent)
        : Instruction(ty, 1, parent) {
        setOperand(0, v);
    }
};

class BinaryOperator : public Instruction {
   public:
    enum BinaryOps { Add, Sub, Mul, SDiv, SRem, Xor };

   private:
    BinaryOps iType;

    BinaryOperator(BinaryOps iType, Value* lhs, Value* rhs, Type* ty,
                   std::string name, BasicBlock* parent)
        : Instruction(ty, 2, parent), iType(iType) {
        ASSERT(getType() == lhs->getType());
        ASSERT(lhs->getType() == rhs->getType());
        setOperand(0, lhs);
        setOperand(1, rhs);
        setName(std::move(name));
    }

   public:
    static BinaryOperator* Create(BinaryOps iType, Value* lhs, Value* rhs,
                                  std::string name, BasicBlock* parent) {
        return new BinaryOperator(iType, lhs, rhs, lhs->getType(), name,
                                  parent);
    }

    // not llvm
    BinaryOps getBinaryOps() const { return iType; }
    Value*    getLHS() const { return getOperand(0); }
    Value*    getRHS() const { return getOperand(1); }

    std::string_view getOpcodeName() const override {
        switch (getBinaryOps()) {
            case Add:
                return "add";
            case Sub:
                return "sub";
            case Mul:
                return "mul";
            case SDiv:
                return "sdiv";
            case SRem:
                return "srem";
            case Xor:
                return "xor";
            default:
                UNREACHABLE();
        }
    }
};

class AllocaInst final : public UnaryInstruction {
    Type* allocatedType;
    AllocaInst(Type* ty, unsigned addrSpace, Value* arraySize, std::string name,
               BasicBlock* parent)
        : UnaryInstruction(PointerType::get(ty->getContext(), addrSpace),
                           arraySize, parent),
          allocatedType(ty) {
        ASSERT(arraySize->getType()->isIntegerTy());
        setName(std::move(name));
    }

   public:
    static AllocaInst* Create(Type* ty, unsigned addrSpace, Value* arraySize,
                              std::string name, BasicBlock* parent) {
        return new AllocaInst{ty, addrSpace, arraySize, std::move(name),
                              parent};
    }

    const Value* getArraySize() const { return getOperand(0); }
    Value*       getArraySize() { return getOperand(0); }

    PointerType* getType() const {
        return static_cast<PointerType*>(Instruction::getType());
    }

    Type* getAllocatedType() const { return allocatedType; }

    std::string_view getOpcodeName() const override { return "alloca"; }
};

class LoadInst final : public UnaryInstruction {
    LoadInst(Type* ty, Value* ptr, std::string name, BasicBlock* parent)
        : UnaryInstruction(ty, ptr, parent) {
        setName(std::move(name));
    }

   public:
    static LoadInst* Create(Type* ty, Value* ptr, std::string name,
                            BasicBlock* parent) {
        return new LoadInst{ty, ptr, name, parent};
    }

    Value*       getPointerOperand() { return getOperand(0); }
    const Value* getPointerOperand() const { return getOperand(0); }
    Type*        getPointerOperandType() const {
        return getPointerOperand()->getType();
    }

    std::string_view getOpcodeName() const override { return "load"; }
};

class StoreInst final : public Instruction {
    StoreInst(Value* val, Value* ptr, BasicBlock* parent)
        : Instruction(Type::getVoidTy(val->getContext()), 2, parent) {
        setOperand(0, val);
        setOperand(1, ptr);
    }

   public:
    static StoreInst* Create(Value* val, Value* ptr, BasicBlock* parent) {
        return new StoreInst{val, ptr, parent};
    }

    Value*       getValueOperand() { return getOperand(0); }
    const Value* getValueOperand() const { return getOperand(0); }

    Type* getValueOperandType() const { return getValueOperand()->getType(); }

    Value*       getPointerOperand() { return getOperand(1); }
    const Value* getPointerOperand() const { return getOperand(1); }

    Type* getPointerOperandType() const {
        return getPointerOperand()->getType();
    }
    std::string_view getOpcodeName() const override { return "store"; }
};

class GetElementPtrInst final : public Instruction {
    Type* sourceElementType;
    Type* resultElementType;

    GetElementPtrInst(Type* pointeeTy, Value* ptr,
                      const std::vector<Value*>& idxList, std::string name,
                      BasicBlock* parent);

   public:
    static GetElementPtrInst* Create(Type* pointeeTy, Value* ptr,
                                     const std::vector<Value*>& idxList,
                                     std::string name, BasicBlock* parent) {
        return new GetElementPtrInst(pointeeTy, ptr, idxList, std::move(name),
                                     parent);
    }

    Type* getSourceElementType() const { return sourceElementType; }
    Type* getResultElementType() const { return resultElementType; }
    void  setSourceElementType(Type* Ty) { sourceElementType = Ty; }
    void  setResultElementType(Type* Ty) { resultElementType = Ty; }

    Value*       getPointerOperand() { return getOperand(0); }
    const Value* getPointerOperand() const { return getOperand(0); }

    op_iterator        idx_begin() { return op_begin() + 1; }
    const_op_interator idx_begin() const { return op_begin() + 1; }
    op_iterator        idx_end() { return op_end(); }
    const_op_interator idx_end() const { return op_end(); }
    op_range           indices() { return op_range{idx_begin(), idx_end()}; }
    const_op_range     indices() const {
        return const_op_range{idx_begin(), idx_end()};
    }

    std::string_view getOpcodeName() const override { return "getelementptr"; }

    static Type* getIndexedType(Type*                     pointeeType,
                                const std::vector<Value*> idxList);
    static Type* getTypeAtIndex(Type* pointeeType, Value* idx);
};

class CmpInst : public Instruction {
   public:
    enum Predicate : unsigned {
        ICMP_EQ  = 32,  ///< equal
        ICMP_NE  = 33,  ///< not equal
        ICMP_SGT = 38,  ///< signed greater than
        ICMP_SGE = 39,  ///< signed greater or equal
        ICMP_SLT = 40,  ///< signed less than
        ICMP_SLE = 41,  ///< signed less or equal
    };

   private:
    Predicate pred;

   protected:
    CmpInst(Type* ty, Predicate pred, Value* lhs, Value* rhs, std::string name,
            BasicBlock* parent)
        : Instruction(ty, 2, parent), pred(pred) {
        setOperand(0, lhs);
        setOperand(1, rhs);
        setName(std::move(name));
    }

   public:
    Predicate getPredicate() const { return pred; }
    void      setPredicate(Predicate p) { pred = p; }

    // 在 llvm ir 中找不到
    Value* getLHS() const { return getOperand(0); }
    Value* getRHS() const { return getOperand(1); }
};

class ICmpInst final : public CmpInst {
    ICmpInst(Predicate pred, Value* lhs, Value* rhs, std::string name,
             BasicBlock* parent)
        : CmpInst(IntegerType::get(lhs->getContext(), 1), pred, lhs, rhs,
                  std::move(name), parent) {}

   public:
    static ICmpInst* Create(Predicate pred, Value* lhs, Value* rhs,
                            std::string name, BasicBlock* parent) {
        return new ICmpInst{pred, lhs, rhs, std::move(name), parent};
    }

    std::string_view getOpcodeName() const override { return "icmp"; }
};

class CallInst : public Instruction {
    FunctionType* funcTy;
    CallInst(FunctionType* ty, Value* func, const std::vector<Value*>& args,
             std::string name, BasicBlock* parent);

   public:
    static CallInst* Create(FunctionType* ty, Value* func,
                            const std::vector<Value*>& args, std::string name,
                            BasicBlock* parent) {
        return new CallInst{ty, func, args, std::move(name), parent};
    }

    FunctionType* getFunctionType() const { return funcTy; }

    op_iterator        arg_begin() { return op_begin(); }
    const_op_interator arg_begin() const { return op_begin(); }
    op_iterator        arg_end() { return op_end() - 1; }
    const_op_interator arg_end() const { return op_end() - 1; }
    op_range           args() { return op_range{arg_begin(), arg_end()}; }
    const_op_range     args() const {
        return const_op_range{arg_begin(), arg_end()};
    }
    size_t arg_size() const { return arg_end() - arg_begin(); }

    Value* getCalledOperand() const { return getOperand(getNumOperands() - 1); }

    std::string_view getOpcodeName() const override { return "call"; }
};

class ReturnInst final : public Instruction {
    ReturnInst(LLVMContext& c, Value* retVal, BasicBlock* parent)
        : Instruction(Type::getVoidTy(c), retVal ? 1 : 0, parent) {
        if (retVal) setOperand(0, retVal);
    }

   public:
    static ReturnInst* Create(LLVMContext& c, Value* retVal,
                              BasicBlock* parent) {
        return new ReturnInst(c, retVal, parent);
    }

    Value* getReturnValue() const {
        return getNumOperands() != 0 ? getOperand(0) : nullptr;
    }

    std::string_view getOpcodeName() const override { return "ret"; }
};

class BranchInst final : public Instruction {
    BranchInst(BasicBlock* ifTrue, BasicBlock* ifFalse, Value* cond,
               BasicBlock* parent);

    BranchInst(BasicBlock* ifTrue, BasicBlock* parent);

   public:
    static BranchInst* Create(BasicBlock* ifTrue, BasicBlock* ifFalse,
                              Value* cond, BasicBlock* parent) {
        return new BranchInst(ifTrue, ifFalse, cond, parent);
    }

    static BranchInst* Create(BasicBlock* ifTrue, BasicBlock* parent) {
        return new BranchInst{ifTrue, parent};
    }

    bool isUnconditional() const { return getNumOperands() == 1; }
    bool isConditional() const { return getNumOperands() == 3; }

    Value* getCondition() const {
        ASSERT_WITH(isConditional(),
                    "Cannot get condition of an uncond branch!");
        return getOperand(0);
    }

    void setCondition(Value* v) {
        ASSERT_WITH(isConditional(),
                    "Cannot set condition of unconditional branch!");
        setOperand(0, v);
    }

    // 以下不是 llvm 中的函数
    BasicBlock* getTrueBB() const;
    BasicBlock* getFalseBB() const;

    std::string_view getOpcodeName() const override { return "br"; }
};

class CastInst : public UnaryInstruction {
   public:
    enum CastOps { SExt = 40 };

   private:
    CastOps ops;

   protected:
    CastInst(CastOps ops, Value* s, Type* ty, std::string name,
             BasicBlock* parent)
        : UnaryInstruction(ty, s, parent), ops(ops) {
        setName(std::move(name));
    }

   public:
    static CastInst* Create(CastOps ops, Value* s, Type* ty, std::string name,
                            BasicBlock* parent);

    CastOps getOpcode() const { return ops; }
    Type*   getSrcTy() const { return getSrc()->getType(); }
    Type*   getDestTy() const { return getType(); }
    // not llvm
    Value* getSrc() const { return getOperand(0); }
};

class SExtInst final : public CastInst {
    SExtInst(Value* s, Type* ty, std::string name, BasicBlock* parent)
        : CastInst(CastInst::SExt, s, ty, std::move(name), parent) {}

   public:
    static SExtInst* Create(Value* s, Type* ty, std::string name,
                            BasicBlock* parent) {
        return new SExtInst{s, ty, std::move(name), parent};
    }

    std::string_view getOpcodeName() const override { return "sext"; }
};
}  // namespace ir
