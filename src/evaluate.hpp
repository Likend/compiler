#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Value.h>

#include "symbol_table.hpp"
#include "token.hpp"
#include "util/assert.hpp"

class Exp {
   public:
    enum Type { T_INT, T_BOOL, T_PTR, T_VOID };
    virtual ~Exp() = default;

    virtual llvm::Value* rvalue(llvm::IRBuilder<>& builder) = 0;
    virtual llvm::Value* lvalue([[maybe_unused]] llvm::IRBuilder<>& buidler) {
        throw std::runtime_error("Error: Expression cannot be used as Lvalue!");
    }
    virtual std::optional<int32_t> test_constexpr() { return std::nullopt; }
    virtual Type type() = 0;
};

class IntLiteralExp : public Exp {
   private:
    int32_t value;

   public:
    IntLiteralExp(int32_t value) : value(value) {}

    llvm::Value* rvalue(llvm::IRBuilder<>& builder) override {
        return builder.getInt32(value);
    }
    std::optional<int32_t> test_constexpr() override { return value; }
    Type type() override { return T_INT; }
};

class IntVarExp : public Exp {
   private:
    const SymbolTable::Record& var_symbol;

   public:
    IntVarExp(const SymbolTable::Record& var_symbol) : var_symbol(var_symbol) {}

    llvm::Value* rvalue(llvm::IRBuilder<>& builder) override;
    llvm::Value* lvalue(llvm::IRBuilder<>& builder) override;
    std::optional<int32_t> test_constexpr() override;
    Type type() override { return T_INT; }
};

class PtrVarExp : public Exp {
   private:
    const SymbolTable::Record& var_symbol;

   public:
    PtrVarExp(const SymbolTable::Record& var_symbol) : var_symbol(var_symbol) {}

    llvm::Value* rvalue(llvm::IRBuilder<>& builder) override;
    Type type() override { return T_PTR; }
};

class FuncCallExp : public Exp {
   private:
    const SymbolTable::Record& func_symbol;
    std::vector<std::unique_ptr<Exp>> params;

   public:
    FuncCallExp(const SymbolTable::Record& func_symbol,
                std::vector<std::unique_ptr<Exp>> params)
        : func_symbol(func_symbol), params(std::move(params)) {
        for (const auto& param : this->params) {
            ASSERT(param);
        }
    }

    llvm::Value* rvalue(llvm::IRBuilder<>& builder) override;
    Type type() override;  // INT | VOID
};

class ArrayAccessExp : public Exp {
   private:
    const SymbolTable::Record& record;
    std::unique_ptr<Exp> index;

   public:
    ArrayAccessExp(const SymbolTable::Record& record,
                   std::unique_ptr<Exp> index)
        : record(record), index(std::move(index)) {
        ASSERT(this->index);
        ASSERT(this->index->type() == T_INT);
    }

    llvm::Value* rvalue(llvm::IRBuilder<>& builder) override;
    llvm::Value* lvalue(llvm::IRBuilder<>& builder) override;
    std::optional<int32_t> test_constexpr() override;
    Type type() override { return T_INT; }
};

class BinaryOpIntExp : public Exp {
    Token::Type op;  // one of '+', '-', '*', '/', '%'
    std::unique_ptr<Exp> lhs, rhs;

   public:
    BinaryOpIntExp(Token::Type op, std::unique_ptr<Exp> lhs,
                   std::unique_ptr<Exp> rhs)
        : op(op), lhs(std::move(lhs)), rhs(std::move(rhs)) {
        ASSERT(this->lhs);
        ASSERT(this->rhs);
        ASSERT(this->lhs->type() == T_INT);
        ASSERT(this->rhs->type() == T_INT);
    }

    llvm::Value* rvalue(llvm::IRBuilder<>& builder) override;
    std::optional<int32_t> test_constexpr() override;
    Type type() override { return T_INT; }
};

class BinaryOpBoolExp : public Exp {
    Token::Type op;  // one of '>', '>=', '<', '<=', '==', '!='
    std::unique_ptr<Exp> lhs, rhs;

   public:
    BinaryOpBoolExp(Token::Type op, std::unique_ptr<Exp> lhs,
                    std::unique_ptr<Exp> rhs)
        : op(op), lhs(std::move(lhs)), rhs(std::move(rhs)) {
        ASSERT(this->lhs);
        ASSERT(this->rhs);
        ASSERT(this->lhs->type() == T_INT);
        ASSERT(this->rhs->type() == T_INT);
    }
    llvm::Value* rvalue(llvm::IRBuilder<>& builder) override;
    Type type() override { return T_BOOL; }
};

class UnaryExp : public Exp {
    Token::Type op;  // one of '+', '-', '!'
    std::unique_ptr<Exp> exp;

   public:
    UnaryExp(Token::Type op, std::unique_ptr<Exp> exp)
        : op(op), exp(std::move(exp)) {
        ASSERT(this->exp);
        ASSERT(this->exp->type() == T_INT);
    }

    llvm::Value* rvalue(llvm::IRBuilder<>& builder) override;
    std::optional<int32_t> test_constexpr() override;
    Type type() override { return T_INT; }
};

class PoisonIntVarExp : public Exp {
   public:
    llvm::Value* rvalue(llvm::IRBuilder<>& builder) override {
        return builder.getInt32(0);
    }
    llvm::Value* lvalue(llvm::IRBuilder<>& builder) override {
        return builder.CreateAlloca(builder.getInt32Ty());
    }
    std::optional<int32_t> test_constexpr() override { return 0; };
    Type type() override { return T_INT; }
};

class IntToBoolExp : public Exp {
   private:
    std::unique_ptr<Exp> int_exp;

   public:
    IntToBoolExp(std::unique_ptr<Exp> int_exp) : int_exp(std::move(int_exp)) {
        ASSERT(this->int_exp);
        ASSERT(this->int_exp->type() == T_INT);
    }
    llvm::Value* rvalue(llvm::IRBuilder<>& builder) override {
        return builder.CreateICmpNE(int_exp->rvalue(builder),
                                    builder.getInt32(0));
    }
    Type type() override { return T_BOOL; }
};

class Cond {
   public:
    virtual ~Cond() = default;

    virtual void gen_code(llvm::BasicBlock* true_bb, llvm::BasicBlock* false_bb,
                          llvm::IRBuilder<>& builder) = 0;
};

class SingleCond : public Cond {
   private:
    std::unique_ptr<Exp> exp;

   public:
    SingleCond(std::unique_ptr<Exp> exp) : exp(std::move(exp)) {
        ASSERT(this->exp);
        ASSERT(this->exp->type() == Exp::T_BOOL);
    }
    void gen_code(llvm::BasicBlock* true_bb, llvm::BasicBlock* false_bb,
                  llvm::IRBuilder<>& builder) override;
};

class BinCondBase : public Cond {
   protected:
    std::unique_ptr<Cond> lhs;
    std::unique_ptr<Cond> rhs;

   public:
    BinCondBase(std::unique_ptr<Cond> lhs, std::unique_ptr<Cond> rhs)
        : lhs(std::move(lhs)), rhs(std::move(rhs)) {
        ASSERT(this->lhs);
        ASSERT(this->rhs);
    }
};

class LAndCond : public BinCondBase {
   public:
    using BinCondBase::BinCondBase;
    void gen_code(llvm::BasicBlock* true_bb, llvm::BasicBlock* false_bb,
                  llvm::IRBuilder<>& builder) override;
};

class LOrCond : public BinCondBase {
   public:
    using BinCondBase::BinCondBase;
    void gen_code(llvm::BasicBlock* true_bb, llvm::BasicBlock* false_bb,
                  llvm::IRBuilder<>& builder) override;
};
