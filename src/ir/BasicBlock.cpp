#include "ir/BasicBlock.hpp"

#include <memory>

#include "ir/Function.hpp"
#include "ir/Instructions.hpp"
#include "ir/Module.hpp"
#include "ir/Type.hpp"
#include "ir/Value.hpp"

using namespace ir;

BasicBlock::BasicBlock(LLVMContext& c, std::string name, Function* parent)
    : Value(Type::getLabelTy(c)), parent(parent) {
    if (parent) {
        parent->basicBlocks.emplace_back(std::unique_ptr<BasicBlock>(this));
    }
    setName(std::move(name));
}
BasicBlock::~BasicBlock() = default;

Module*       BasicBlock::getModule() { return getParent()->getParent(); }
const Module* BasicBlock::getModule() const { return getParent()->getParent(); }
