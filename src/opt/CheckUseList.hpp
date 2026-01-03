#pragma once

#include "ir/Function.hpp"
#include "ir/Module.hpp"
#include "ir/Pass.hpp"
namespace opt {
class CheckUseListPass final : public ir::Pass {
   public:
    bool runOnModule(ir::Module&) override;
    bool runOnFunction(ir::Function&);
};
}  // namespace opt
