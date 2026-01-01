#pragma once

#include <algorithm>
#include <vector>

#include "util/assert.hpp"
#include "util/iterator_range.hpp"

namespace codegen {
class MachineInstr;
class MachineOperand;

class RegisterInfo {
   public:
    std::vector<MachineOperand*> useList;
    std::vector<MachineOperand*> defList;

    void addUse(MachineOperand& op) { useList.push_back(&op); }
    void addDef(MachineOperand& op) { defList.push_back(&op); }
    void removeUse(MachineOperand& op) {
        auto it = std::find(useList.begin(), useList.end(), &op);
        ASSERT(it != useList.end());
        useList.erase(it);
    }
    void removeDef(MachineOperand& op) {
        auto it = std::find(defList.begin(), defList.end(), &op);
        ASSERT(it != defList.end());
        defList.erase(it);
    }

    using mop_iterator       = std::vector<MachineOperand*>::iterator;
    using const_mop_iterator = std::vector<MachineOperand*>::const_iterator;
    using mop_range          = iterator_range<mop_iterator>;
    using const_mop_range    = iterator_range<const_mop_iterator>;

    // clang-format off
    mop_iterator       use_begin()       { return useList.begin(); }
    const_mop_iterator use_begin() const { return useList.begin(); }
    mop_iterator       use_end()         { return useList.end(); }
    const_mop_iterator use_end()   const { return useList.end(); }
    mop_range          use()             { return {use_begin(), use_end()}; }
    const_mop_range    use()       const { return {use_begin(), use_end()}; }
    bool               use_empty() const { return useList.empty(); }

    mop_iterator       def_begin()       { return defList.begin(); }
    const_mop_iterator def_begin() const { return defList.begin(); }
    mop_iterator       def_end()         { return defList.end(); }
    const_mop_iterator def_end()   const { return defList.end(); }
    mop_range          def()             { return {def_begin(), def_end()}; }
    const_mop_range    def()       const { return {def_begin(), def_end()}; }
    bool               def_empty() const { return defList.empty(); }
    // clang-format on
};

}  // namespace codegen
