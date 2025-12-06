#include "ir/Use.hpp"

using namespace ir;

void Use::set(Value* val) {
    removeFromList();
    this->val = val;
    if (val) val->addUse(*this);
}

void Use::addToList(Use** list) {
    next = *list;
    if (next) next->prev = &next;
    prev  = list;
    *prev = this;
}

void Use::removeFromList() {
    if (prev) {  // 如果 prev 为空, 则该 use 没有被加入任何列表
        *prev = next;
        if (next) {
            next->prev = prev;
            next       = nullptr;
        }
        prev = nullptr;
    }
}
