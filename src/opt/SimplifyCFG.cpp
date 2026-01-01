#include "opt/SimplifyCFG.hpp"

#include <queue>
#include <unordered_set>

#include "ir/BasicBlock.hpp"
#include "ir/Function.hpp"
#include "ir/Instructions.hpp"
#include "ir/Pass.hpp"
#include "util/assert.hpp"
#include "util/IntrusiveList.hpp"

using namespace opt;

template <typename Range>
static typename Range::value_type* getSingleItem(const Range& rg) {
    auto it = rg.begin();
    if (it == rg.end()) return nullptr;
    typename Range::value_type* item = &*it;
    ++it;
    if (it == rg.end())
        return item;
    else
        return nullptr;
}

/// One item can be appear multiple times
template <typename Range>
static typename Range::value_type* getUniqueItem(const Range& rg) {
    auto it = rg.begin();
    if (it == rg.end()) return nullptr;
    typename Range::value_type* item = &*it;
    ++it;
    for (; it != rg.end(); ++it) {
        if (&*it != item) return nullptr;
    }
    return item;
}

static bool RemoveUnreachableBlocks(ir::Function& f) {
    bool changed = false;

    std::unordered_set<ir::BasicBlock*> reachable;
    std::queue<ir::BasicBlock*>         worklist;

    ir::BasicBlock* bb = &f.getEntryBlock();
    worklist.push(bb);
    do {
        bb = worklist.front();
        reachable.insert(bb);
        worklist.pop();
        for (ir::BasicBlock& successor : bb->successors()) {
            if (reachable.find(&successor) == reachable.end())
                worklist.push(&successor);
        }
    } while (!worklist.empty());

    if (reachable.size() == f.size()) return changed;

    ASSERT(reachable.size() < f.size());

    for (ir::Function::iterator it = f.begin(); it != f.end();) {
        ir::BasicBlock& bb = *it;
        if (reachable.find(&bb) != reachable.end()) {
            ++it;
            continue;
        }
        for (ir::Instruction& inst : bb) {
            inst.dropAllReferences();
        }
        it      = f.erase(it);
        changed = true;
    }

    return changed;
}

/// Attempts to merge a block into its predecessor, if there is only one
/// predecessor and there if only one successor of the predecessor.
static bool MergeBlockIntoPredecessor(ir::BasicBlock* bb) {
    // Can't merge if there are multiple predecessors, or no predecessors.
    ir::BasicBlock* pred = getUniqueItem(bb->predecessors());
    if (!pred) return false;

    // Don't break self-loops.
    if (pred == bb) return false;

    // Can't merge if there are multiple distinct successors.
    if (getUniqueItem(pred->successors()) != bb) return false;

    // Can't merge if there is PHI loop.
    // TODO: PHI

    // Begin by getting rid of unneeded PHIs.
    // TODO: PHI

    // Delete the unconditional branch from the predecessor.
    pred->getTerminator()->dropAllReferences();
    pred->erase(pred->getTerminator());

    // Move all definitions in the successor to the predecessor...
    pred->splice(pred->end(), *bb);

    // Make all PHI nodes that referred to BB now refer to Pred as their
    // source...
    bb->replaceAllUsesWith(pred);

    // Finally, erase the old block.
    bb->parent()->erase(bb);
    return false;
}

static bool simplifyOnce(ir::BasicBlock* bb) {
    bool changed = false;
    ASSERT_WITH(bb && bb->parent(), "Block not embedded in function!");
    ASSERT_WITH(bb->getTerminator(), "Degenerate basic block encountered!");

    // Remove basic blocks that have no predecessors (except the entry
    // block)... or that just have themself as a predecessor.  These are
    // unreachable.
    if ((bb->pred_empty() && bb != &bb->parent()->getEntryBlock()) ||
        getSingleItem(bb->predecessors()) == bb) {
        // Delete dead block
        IntrusiveIterator<ir::BasicBlock> it{*bb};
        bb->parent()->erase(it);
        return true;
    }

    // Check to see if we can constant propagate this terminator instruction
    // away...
    // TODO

    // Merge basic blocks into their predecessor if there is only one
    // distinct pred, and if there is only one distinct successor of the
    // predecessor, and if there are no PHI nodes.
    if (MergeBlockIntoPredecessor(bb)) return true;

    // 指令下沉

    // ir::TerminatorInst* terminator = bb->getTerminator();
    // if (auto* i = dynamic_cast<ir::BranchInst*>(terminator)) {
    //     changed |= simplifyBranch(i);
    // }

    // todo
    return changed;
}

static bool simplifyCFG(ir::BasicBlock* bb) {
    bool changed = false;
    bool resimplfy;
    do {
        resimplfy = simplifyOnce(bb);
        changed |= resimplfy;
    } while (resimplfy);
    return changed;
}

static bool iterativelySimplifyCFG(ir::Function& f) {
    bool Changed     = false;
    bool LocalChange = true;

    while (LocalChange) {
        LocalChange = false;
        for (ir::Function::iterator bbit = f.begin(); bbit != f.end();) {
            ir::BasicBlock& bb = *bbit++;
            if (simplifyCFG(&bb)) {
                LocalChange = true;
            }
        }
        Changed |= LocalChange;
    }
    return Changed;
}

bool SimplifyCFGPass::runOnFunction(ir::Function& f) {
    if (f.empty()) return false;

    bool changed = false;

    changed |= RemoveUnreachableBlocks(f);
    changed |= iterativelySimplifyCFG(f);

    if (!changed) return false;

    if (!RemoveUnreachableBlocks(f)) return true;

    do {
        changed = iterativelySimplifyCFG(f);
        changed |= RemoveUnreachableBlocks(f);
    } while (changed);

    return true;
}
