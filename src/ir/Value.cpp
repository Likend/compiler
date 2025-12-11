#include "ir/Value.hpp"

#include "ir/BasicBlock.hpp"
#include "ir/Function.hpp"
#include "ir/Instructions.hpp"
#include "ir/LLVMContext.hpp"
#include "ir/Module.hpp"
#include "ir/Type.hpp"
#include "ir/Use.hpp"
#include "ir/ValueSymbolTable.hpp"

using namespace ir;

void Value::addUse(Use& use) { use.addToList(&useList); }

LLVMContext& Value::getContext() const { return ty->getContext(); }

static ValueSymbolTable* getSymTab(Value* V) {
    if (auto* I = dynamic_cast<Instruction*>(V)) {
        if (BasicBlock* P = I->getParent())
            if (Function* PP = P->getParent())
                return &PP->getValueSymbolTable();
    } else if (auto* BB = dynamic_cast<BasicBlock*>(V)) {
        if (Function* P = BB->getParent()) return &P->getValueSymbolTable();
    } else if (auto* GV = dynamic_cast<GlobalValue*>(V)) {
        if (Module* P = GV->getParent()) return &P->getValueSymbolTable();
    } else if (auto* A = dynamic_cast<Argument*>(V)) {
        if (Function* P = A->getParent()) return &P->getValueSymbolTable();
    }
    return nullptr;
}

void Value::setName(std::string name) {
    ASSERT_WITH(name.empty() || !getType()->isVoidTy(),
                "Cannot assign a name to void values!");

    auto* symTab = getSymTab(this);
    if (symTab) name = symTab->makeUniqueName(std::move(name));
    this->name = std::move(name);
    if (symTab) symTab->insertValue(this);
}
