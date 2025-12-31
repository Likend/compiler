#include "ir/BasicBlock.hpp"

#include "ir/Function.hpp"
#include "ir/Instructions.hpp"
#include "ir/Module.hpp"

using namespace ir;

Module&       BasicBlock::getModule() { return *parent()->parent(); }
const Module& BasicBlock::getModule() const { return *parent()->parent(); }

ir::BasicBlock* ir::BasicBlock::Create(LLVMContext& c, std::string name,
                                       Function* parent) {
    auto* bb = new BasicBlock{c, std::move(name)};
    if (parent) parent->push_back(bb);
    return bb;
}
