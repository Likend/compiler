#pragma once

#include "ir/Pass.hpp"
namespace opt {
class FunctionInlinePass final : public ir::Pass {
   public:
    bool runOnModule(ir::Module&) override;
};
}  // namespace opt
