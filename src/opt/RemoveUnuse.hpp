#pragma once

#include "ir/BasicBlock.hpp"
#include "ir/Pass.hpp"
namespace opt {
class RemoveUnusePass final : public ir::FunctionPass {
   public:
    bool runOnFunction(ir::Function& f) override;
};
}  // namespace opt
