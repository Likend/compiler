#pragma once

#include <ostream>

#include "ir/Pass.hpp"

namespace ir {
class Module;

class IRWriterPass final : public ImmutablePass {
   public:
    IRWriterPass(std::ostream& os) : os(os) {}

    void doInitialization(Module&) override;

   private:
    std::ostream& os;
};
};  // namespace ir
