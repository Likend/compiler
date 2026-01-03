#include "opt/CheckUseList.hpp"

#include <iostream>
#include <unordered_set>

#include "ir/Function.hpp"
#include "ir/Module.hpp"

using namespace ir;
using namespace opt;

void verifyValueUsers(Value* value, const std::unordered_set<Value*>& values) {
    for (auto& user : value->users()) {
        if (values.count(&user) == 0) {
            std::cerr << "Value " << value->getName() << " " << value->getType()
                      << " user not in instructions";
        }
    }
}

bool CheckUseListPass::runOnModule(Module& m) {
    std::unordered_set<Value*> values;

    for (Function& f : m) {
        for (BasicBlock& bb : f) {
            for (Instruction& inst : bb) {
                values.insert(&inst);
            }
        }
    }

    for (Function& f : m) {
        verifyValueUsers(&f, values);
    }

    for (Function& f : m) {
        runOnFunction(f);
    }

    return false;
}

bool CheckUseListPass::runOnFunction(Function& f) {
    std::unordered_set<Value*> values;

    for (BasicBlock& bb : f) {
        for (Instruction& inst : bb) {
            values.insert(&inst);
        }
    }

    for (BasicBlock& bb : f) {
        verifyValueUsers(&bb, values);
        for (Instruction& inst : bb) {
            verifyValueUsers(&inst, values);
        }
    }
    return false;
}
