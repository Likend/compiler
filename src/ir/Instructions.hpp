#pragma once

#include <cstddef>
#include <functional>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ir/LLVMContext.hpp"
#include "ir/Type.hpp"
#include "ir/User.hpp"
#include "ir/ValueSymbolTable.hpp"
#include "util/assert.hpp"
#include "util/iterator_range.hpp"

namespace ir {
class BasicBlock;

class Instruction : public User, public ValueNode<Instruction, BasicBlock> {
   protected:
    Instruction(Type* ty, size_t numOperands) : User(ty, numOperands) {}

   public:
    Instruction(const Instruction&)            = delete;
    Instruction& operator=(const Instruction&) = delete;

    virtual bool             isTerminator() const { return false; }
    virtual std::string_view getOpcodeName() const = 0;
    virtual Instruction*     clone(
            const std::function<Value*(const Value*)>& vmap_lookup) const = 0;
};

class UnaryInstruction : public Instruction {
   protected:
    UnaryInstruction(Type* ty, Value* v) : Instruction(ty, 1) {
        setOperand(0, v);
    }
};

class BinaryOperator final : public Instruction {
   public:
    enum BinaryOps { Add, Sub, Mul, SDiv, SRem, Xor };

   private:
    BinaryOps iType;

   public:
    BinaryOperator(BinaryOps iType, Value* lhs, Value* rhs, Type* ty,
                   std::string name)
        : Instruction(ty, 2), iType(iType) {
        ASSERT(getType() == lhs->getType());
        ASSERT(lhs->getType() == rhs->getType());
        setOperand(0, lhs);
        setOperand(1, rhs);
        setName(std::move(name));
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

    // Associative operators satisfy:  x op (y op z) === (x op y) op z
    bool isAssociative() const {
        return iType == Add || iType == Mul || iType == Xor;
    }

    // Commutative operators satisfy: (x op y) === (y op x)
    bool isCommutative() const {
        return iType == Add || iType == Mul || iType == Xor;
    }

    BinaryOperator* clone(
        const std::function<Value*(const Value*)>& vmap_lookup) const override {
        return new BinaryOperator{iType, vmap_lookup(getLHS()),
                                  vmap_lookup(getRHS()), getType(),
                                  std::string(getName())};
    }
};

class AllocaInst final : public UnaryInstruction {
    Type* allocatedType;

   public:
    AllocaInst(Type* ty, unsigned addrSpace, Value* arraySize, std::string name)
        : UnaryInstruction(PointerType::get(ty->getContext(), addrSpace),
                           arraySize),
          allocatedType(ty) {
        ASSERT(arraySize->getType()->isIntegerTy());
        setName(std::move(name));
    }

    const Value* getArraySize() const { return getOperand(0); }
    Value*       getArraySize() { return getOperand(0); }

    PointerType* getType() const {
        return static_cast<PointerType*>(Instruction::getType());
    }

    Type* getAllocatedType() const { return allocatedType; }

    std::string_view getOpcodeName() const override { return "alloca"; }

    AllocaInst* clone(
        const std::function<Value*(const Value*)>& vmap_lookup) const override {
        return new AllocaInst{getAllocatedType(), getType()->getAddrSpace(),
                              vmap_lookup(getArraySize()), std::string(name)};
    }
};

class LoadInst final : public UnaryInstruction {
   public:
    LoadInst(Type* ty, Value* ptr, std::string name)
        : UnaryInstruction(ty, ptr) {
        setName(std::move(name));
    }

    Value*       getPointerOperand() { return getOperand(0); }
    const Value* getPointerOperand() const { return getOperand(0); }
    Type*        getPointerOperandType() const {
        return getPointerOperand()->getType();
    }

    std::string_view getOpcodeName() const override { return "load"; }

    LoadInst* clone(
        const std::function<Value*(const Value*)>& vmap_lookup) const override {
        return new LoadInst{getType(), vmap_lookup(getPointerOperand()),
                            std::string(name)};
    }
};

class StoreInst final : public Instruction {
   public:
    StoreInst(Value* val, Value* ptr)
        : Instruction(Type::getVoidTy(val->getContext()), 2) {
        setOperand(0, val);
        setOperand(1, ptr);
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

    StoreInst* clone(
        const std::function<Value*(const Value*)>& vmap_lookup) const override {
        return new StoreInst{vmap_lookup(getValueOperand()),
                             vmap_lookup(getPointerOperand())};
    }
};

class GetElementPtrInst final : public Instruction {
    Type* sourceElementType;
    Type* resultElementType;

   public:
    GetElementPtrInst(Type* pointeeTy, Value* ptr,
                      const std::vector<Value*>& idxList, std::string name);

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

    static Type* getIndexedType(Type*                      pointeeType,
                                const std::vector<Value*>& idxList);
    static Type* getTypeAtIndex(Type* pointeeType, Value* idx);

    GetElementPtrInst* clone(
        const std::function<Value*(const Value*)>& vmap_lookup) const override;
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
    CmpInst(Type* ty, Predicate pred, Value* lhs, Value* rhs, std::string name)
        : Instruction(ty, 2), pred(pred) {
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
   public:
    ICmpInst(Predicate pred, Value* lhs, Value* rhs, std::string name)
        : CmpInst(IntegerType::get(lhs->getContext(), 1), pred, lhs, rhs,
                  std::move(name)) {}

    std::string_view getOpcodeName() const override { return "icmp"; }

    ICmpInst* clone(
        const std::function<Value*(const Value*)>& vmap_lookup) const override {
        return new ICmpInst{getPredicate(), vmap_lookup(getLHS()),
                            vmap_lookup(getRHS()), std::string(getName())};
    }
};

class CallInst final : public Instruction {
    FunctionType* funcTy;

   public:
    CallInst(FunctionType* ty, Value* func, const std::vector<Value*>& args,
             std::string name);

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

    CallInst* clone(
        const std::function<Value*(const Value*)>& vmap_lookup) const override;
};

class TerminatorInst : public Instruction {
   public:
    using Instruction::Instruction;

    bool isTerminator() const override { return true; }

    virtual unsigned    getNumSuccessors() const       = 0;
    virtual BasicBlock* getSuccessor(unsigned i) const = 0;

   private:
    template <typename TerminatorInstT, typename BasicBlockT>
    struct succ_iterator_impl {
        using iterator_category = std::random_access_iterator_tag;
        using difference_type   = int;
        using value_type        = BasicBlockT;
        using pointer           = value_type*;
        using reference         = value_type&;

       private:
        using Self = succ_iterator_impl;

       public:
        succ_iterator_impl() = default;

       private:
        friend class TerminatorInst;
        succ_iterator_impl(TerminatorInstT* inst, unsigned idx = 0)
            : inst(inst), idx(static_cast<int>(idx)) {}

       public:
        reference operator*() const { return *inst->getSuccessor(idx); }
        pointer   operator->() const { return &operator*(); }
        bool      operator<(const Self& rhs) const { return idx < rhs.idx; }
        bool      operator==(const Self& rhs) const { return idx == rhs.idx; }
        bool      operator!=(const Self& rhs) const { return idx != rhs.idx; }

        difference_type operator-(const Self& rhs) const {
            return idx - rhs.idx;
        }

        Self& operator+=(difference_type n) {
            idx += n;
            ASSERT_WITH(index_is_valid(), "Iterator index out of bound");
            return *this;
        }
        Self& operator-=(difference_type n) { return operator+=(-n); }

        Self operator+(difference_type n) {
            Self tmp = *this;
            this += n;
            return this;
        }

        Self operator-(difference_type n) {
            Self tmp = *this;
            this -= n;
            return this;
        }

        Self& operator++() { return operator+=(1); }
        Self& operator--() { return operator-=(1); }

        Self operator++(int) {
            Self tmp = *this;
            ++*this;
            return tmp;
        }

        Self operator--(int) {
            Self tmp = *this;
            --*this;
            return tmp;
        }

       private:
        TerminatorInstT* inst = nullptr;
        int              idx  = 0;

        bool index_is_valid() {
            return idx >= 0 &&
                   static_cast<unsigned>(idx) <= inst->getNumSuccessors();
        }
    };

   public:
    // clang-format off
    using succ_iterator       = succ_iterator_impl<TerminatorInst, BasicBlock>;
    using const_succ_iterator = succ_iterator_impl<const Instruction, const BasicBlock>;
    using succ_range          = iterator_range<succ_iterator>;
    using const_succ_range    = iterator_range<const_succ_iterator>;

    succ_iterator       succ_begin()       { return {this}; }
    const_succ_iterator succ_begin() const { return {this}; }
    succ_iterator       succ_end()         { return {this, getNumSuccessors()}; }
    const_succ_iterator succ_end()   const { return {this, getNumSuccessors()}; }
    succ_range          successors()       { return {succ_begin(), succ_end()}; }
    const_succ_range    successors() const { return {succ_begin(), succ_end()}; }
    bool                succ_empty() const { return getNumSuccessors() == 0; }
    size_t              succ_size()  const { return getNumSuccessors(); }
    // clang-format on
};

class ReturnInst final : public TerminatorInst {
   public:
    ReturnInst(LLVMContext& c, Value* retVal)
        : TerminatorInst(Type::getVoidTy(c), retVal ? 1 : 0) {
        if (retVal) setOperand(0, retVal);
    }

    Value* getReturnValue() const {
        return getNumOperands() != 0 ? getOperand(0) : nullptr;
    }

    bool isTerminator() const override { return true; }

    std::string_view getOpcodeName() const override { return "ret"; }

    unsigned    getNumSuccessors() const override { return 0; }
    BasicBlock* getSuccessor(unsigned) const override { UNREACHABLE(); }

    ReturnInst* clone(
        const std::function<Value*(const Value*)>& vmap_lookup) const override {
        return new ReturnInst{getContext(), vmap_lookup(getReturnValue())};
    }
};

class BranchInst final : public TerminatorInst {
   public:
    BranchInst(BasicBlock* ifTrue, BasicBlock* ifFalse, Value* cond);

    BranchInst(BasicBlock* ifTrue);

    bool isUnconditional() const { return getNumOperands() == 1; }
    bool isConditional() const { return getNumOperands() == 3; }

    Value* getCondition() const {
        ASSERT_WITH(isConditional(),
                    "Cannot get condition of an uncond branch!");
        return getOperand(2);
    }

    void setCondition(Value* v) {
        ASSERT_WITH(isConditional(),
                    "Cannot set condition of unconditional branch!");
        setOperand(2, v);
    }

    BasicBlock* getTrueBB() const;
    BasicBlock* getFalseBB() const;

    bool isTerminator() const override { return true; }

    std::string_view getOpcodeName() const override { return "br"; }

    unsigned getNumSuccessors() const override { return 1 + isConditional(); }
    BasicBlock* getSuccessor(unsigned i) const override;

    BranchInst* clone(
        const std::function<Value*(const Value*)>& vmap_lookup) const override;
};

class CastInst final : public UnaryInstruction {
   public:
    enum CastOps {
        SExt = 40,
        ZExt = 41,
    };

   private:
    CastOps ops;

   public:
    CastInst(CastOps ops, Value* s, Type* ty, std::string name)
        : UnaryInstruction(ty, s), ops(ops) {
        setName(std::move(name));
    }

    CastOps getOpcode() const { return ops; }
    Type*   getSrcTy() const { return getSrc()->getType(); }
    Type*   getDestTy() const { return getType(); }
    // not llvm
    Value* getSrc() const { return getOperand(0); }

    std::string_view getOpcodeName() const override {
        switch (ops) {
            case SExt:
                return "sext";
            case ZExt:
                return "zext";
                DEFAULT_UNREACHABLE();
        }
    }

    CastInst* clone(
        const std::function<Value*(const Value*)>& vmap_lookup) const override {
        return new CastInst{getOpcode(), vmap_lookup(getSrc()), getType(),
                            std::string(getName())};
    }
};

class PHINode final : public Instruction {
    /// The number of operands actually allocated.  NumOperands is
    /// the number actually in use.
    std::vector<BasicBlock*> blocksList;

    size_t ReservedSpace;

   public:
    PHINode(Type* ty, size_t numberPreservedValues, std::string name)
        : Instruction(ty, numberPreservedValues),
          ReservedSpace(numberPreservedValues) {
        blocksList.reserve(numberPreservedValues);
        setName(std::move(name));
    }

    using const_block_iterator = decltype(blocksList)::const_iterator;

    const_block_iterator block_begin() const { return blocksList.begin(); }
    const_block_iterator block_end() const { return blocksList.end(); }
    iterator_range<const_block_iterator> blocks() const {
        return {block_begin(), block_end()};
    }

    op_range       incoming_values() { return operands(); }
    const_op_range incoming_values() const { return operands(); }

    size_t getNumIncomingValues() const { return getNumOperands(); }

    void   setIncomingValue(size_t i, Value* v) { setOperand(i, v); }
    Value* getIncomingValue(size_t i) const { return getOperand(i); }

    void setIncomingBlock(size_t i, BasicBlock* bb) { blocksList.at(i) = bb; }
    BasicBlock* getIncomingBlock(size_t i) const { return blocksList.at(i); }

    void addIncoming(Value* value, BasicBlock* bb) {
        if (getNumOperands() == ReservedSpace) {
            ReservedSpace = ReservedSpace + ReservedSpace / 2;
            if (ReservedSpace < 2) ReservedSpace = 2;
            growUses(ReservedSpace);
            blocksList.reserve(ReservedSpace);
        }
        setIncomingBlock(getNumOperands() - 1, bb);
        setIncomingValue(getNumOperands() - 1, value);
    }

    // Replace every incoming basic block  Old to basic block  New.
    void replaceIncomingBlockWith(const BasicBlock* Old, BasicBlock* New) {
        ASSERT_WITH(New && Old, "PHI node got a null basic block!");
        for (size_t Op = 0, NumOps = getNumOperands(); Op != NumOps; ++Op)
            if (getIncomingBlock(Op) == Old) setIncomingBlock(Op, New);
    }

    size_t getBasicBlockIndex(const BasicBlock* bb) const {
        for (size_t i = 0, e = getNumOperands(); i != e; ++i)
            if (getIncomingBlock(i) == bb) return i;
        return -1;
    }

    Value* getIncomingValueForBlock(const BasicBlock* bb) const {
        size_t idx = getBasicBlockIndex(bb);
        ASSERT_WITH(idx != static_cast<size_t>(-1),
                    "Invalid basic block argument!");
        return getIncomingValue(idx);
    }

    Value* removeIncomingValue(size_t idx) {
        Value* removed = getIncomingValue(idx);
        operandList[idx].set(nullptr);
        operandList.erase(op_begin() + static_cast<long long>(idx));
        blocksList.erase(block_begin() + static_cast<long long>(idx));
        // TODO maybe delete if empty?
        return removed;
    }

    Value* removeIncomingValue(const BasicBlock* bb) {
        size_t idx = getBasicBlockIndex(bb);
        ASSERT_WITH(idx != static_cast<size_t>(-1),
                    "Invalid basic block argument!");
        return removeIncomingValue(idx);
    }

    /// 每一个前序基本块都有值到当前 PHI
    bool isComplete() const;

    std::string_view getOpcodeName() const override { return "phi"; }

    PHINode* clone(
        const std::function<Value*(const Value*)>& vmap_lookup) const override;
};
}  // namespace ir
