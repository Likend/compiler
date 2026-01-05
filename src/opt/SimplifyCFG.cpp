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
using namespace ir;

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

static bool RemoveUnreachableBlocks(Function& f) {
    // find reachable branches
    std::unordered_set<BasicBlock*> reachable;
    std::queue<BasicBlock*>         worklist;
    worklist.push(&f.front());
    reachable.insert(&f.front());

    do {
        BasicBlock* bb = worklist.front();
        worklist.pop();

        for (BasicBlock& succ : bb->successors()) {
            if (reachable.count(&succ) == 0) {
                worklist.push(&succ);
                reachable.insert(&succ);
            }
        }
    } while (!worklist.empty());

    bool changed = false;

    for (auto bbIt = f.begin(); bbIt != f.end();) {
        BasicBlock& bb = *bbIt++;
        if (reachable.count(&bb) == 0) {
            for (auto instIt = bb.begin(); instIt != bb.end();) {
                Instruction& inst = *instIt;
                inst.replaceAllUsesWith(PoisonValue::get(inst.getType()));
                inst.dropAllReferences();
                instIt = bb.erase(instIt);
            }
            bb.replaceAllUsesWith(PoisonValue::get(bb.getType()));
            f.erase(bb);
            changed = true;
        }
    }

    return changed;
}

/// Attempts to merge a block into its predecessor, if there is only one
/// predecessor and there if only one successor of the predecessor.
static bool MergeBlockIntoPredecessor(BasicBlock* bb) {
    // Can't merge if there are multiple predecessors, or no predecessors.
    BasicBlock* pred = getUniqueItem(bb->predecessors());
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

static bool simplifyOnce(BasicBlock* bb) {
    bool changed = false;
    ASSERT_WITH(bb && bb->parent(), "Block not embedded in function!");
    ASSERT_WITH(bb->getTerminator(), "Degenerate basic block encountered!");

    // Remove basic blocks that have no predecessors (except the entry
    // block)... or that just have themself as a predecessor.  These are
    // unreachable.
    if ((bb->pred_empty() && bb != &bb->parent()->getEntryBlock()) ||
        getSingleItem(bb->predecessors()) == bb) {
        // Delete dead block
        IntrusiveIterator<BasicBlock> it{*bb};
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

    // TerminatorInst* terminator = bb->getTerminator();
    // if (auto* i = dynamic_cast<BranchInst*>(terminator)) {
    //     changed |= simplifyBranch(i);
    // }

    // todo
    return changed;
}

static bool simplifyCFG(BasicBlock* bb) {
    bool changed = false;
    bool resimplfy;
    do {
        resimplfy = simplifyOnce(bb);
        changed |= resimplfy;
    } while (resimplfy);
    return changed;
}

static bool iterativelySimplifyCFG(Function& f) {
    bool Changed     = false;
    bool LocalChange = true;

    while (LocalChange) {
        LocalChange = false;
        for (Function::iterator bbit = f.begin(); bbit != f.end();) {
            BasicBlock& bb = *bbit++;
            if (simplifyCFG(&bb)) {
                LocalChange = true;
            }
        }
        Changed |= LocalChange;
    }
    return Changed;
}

bool SimplifyCFGPass::runOnFunction(Function& f) {
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
