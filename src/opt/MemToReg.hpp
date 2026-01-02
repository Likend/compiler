#pragma once

#include "ir/BasicBlock.hpp"
#include "ir/Pass.hpp"
namespace opt {
class MemToRegPass final : public ir::FunctionPass {
   public:
    bool runOnFunction(ir::Function&) override;

   private:
    bool replaceInnerBasicBlock(ir::BasicBlock&);
};
}  // namespace opt
