#pragma once

#include <iterator>

#include "ir/Instructions.hpp"
#include "ir/LLVMContext.hpp"
#include "ir/User.hpp"
#include "ir/Value.hpp"
#include "ir/ValueSymbolTable.hpp"
#include "util/IntrusiveList.hpp"
#include "util/iterator_range.hpp"
#include "util/iterator_transform.hpp"

namespace ir {
class Function;

class BasicBlock final : public Value,
                         public ValueNode<BasicBlock, Function>,
                         public IntrusiveList<Instruction> {
    friend class Function;
    friend class Instruction;

   public:
    explicit BasicBlock(LLVMContext& c, std::string name = "")
        : Value(Type::getLabelTy(c)), IntrusiveList<Instruction>(*this) {
        setName(std::move(name));
    }

    static BasicBlock* Create(LLVMContext& c, std::string name,
                              Function* parent = nullptr);

    BasicBlock(const BasicBlock&)            = delete;
    BasicBlock& operator=(const BasicBlock&) = delete;

    const Module& getModule() const;
    Module&       getModule();

    /// Returns the terminator instruction if the block is well formed or
    /// null if the block is not well formed.
    const TerminatorInst* getTerminator() const {
        if (empty() || !back().isTerminator()) return nullptr;
        return &dynamic_cast<const TerminatorInst&>(back());
    }
    TerminatorInst* getTerminator() {
        if (empty() || !back().isTerminator()) return nullptr;
        return &dynamic_cast<TerminatorInst&>(back());
    }

    template <typename BasicBlockT, typename UserIteratorT>
    struct pred_iterator_impl
        : public iterator_transform<
              pred_iterator_impl<BasicBlockT, UserIteratorT>, UserIteratorT,
              BasicBlockT> {
       public:
        BasicBlockT& transform(
            typename std::iterator_traits<UserIteratorT>::value_type& user)
            const {
            return *dynamic_cast<Instruction&>(user).parent();
        }
    };

    struct pred_iterator
        : iterator_transform<pred_iterator, user_iterator, BasicBlock> {
       public:
        BasicBlock& transform(User& user) const {
            return *dynamic_cast<Instruction&>(user).parent();
        }
    };

    struct const_pred_iterator
        : iterator_transform<const_pred_iterator, const_user_iterator,
                             const BasicBlock> {
       public:
        const BasicBlock& transform(const User& user) const {
            return *dynamic_cast<const Instruction&>(user).parent();
        }
    };

    using pred_range       = iterator_range<pred_iterator>;
    using const_pred_range = iterator_range<const_pred_iterator>;

    // clang-format off
    pred_iterator       pred_begin()         { return {user_begin()}; }
    const_pred_iterator pred_begin()   const { return {user_begin()}; }
    pred_iterator       pred_end()           { return {user_end()}; }
    const_pred_iterator pred_end()     const { return {user_end()}; }
    pred_range          predecessors()       { return {pred_begin(), pred_end()}; }
    const_pred_range    predecessors() const { return {pred_begin(), pred_end()}; }
    bool                pred_empty()   const { return use_empty(); }

    using succ_iterator       = TerminatorInst::succ_iterator;
    using const_succ_iterator = TerminatorInst::const_succ_iterator;
    using succ_range          = TerminatorInst::succ_range;
    using const_succ_range    = TerminatorInst::const_succ_range;

    succ_iterator       succ_begin()       { return getTerminator()->succ_begin(); }
    const_succ_iterator succ_begin() const { return getTerminator()->succ_begin(); }
    succ_iterator       succ_end()         { return getTerminator()->succ_end(); }
    const_succ_iterator succ_end()   const { return getTerminator()->succ_end(); }
    succ_range          successors()       { return getTerminator()->successors(); }
    const_succ_range    successors() const { return getTerminator()->successors(); }
    bool                succ_empty() const { return getTerminator()->succ_empty(); }
    size_t              succ_size()  const { return getTerminator()->succ_size(); }
    // clang-format on
};
}  // namespace ir
