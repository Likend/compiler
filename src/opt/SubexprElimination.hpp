#pragma once

#include "ir/Function.hpp"
#include "ir/Pass.hpp"

namespace opt {
class SubexprEliminationPass final : public ir::FunctionPass {
   public:
    bool runOnFunction(ir::Function&) override;
};
}  // namespace opt
