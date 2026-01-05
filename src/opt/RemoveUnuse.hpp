#pragma once

#include "ir/BasicBlock.hpp"
#include "ir/Function.hpp"
#include "ir/Pass.hpp"
namespace opt {
class RemoveUnusePass final : public ir::FunctionPass {
   public:
    bool runOnFunction(ir::Function& f) override;

   private:
    bool RemoveUnreachableBranch(ir::Function& f);
};
}  // namespace opt
