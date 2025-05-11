#include "../../include/DataStructures/AVLTree.hpp"

#include <stddef.h>
#include <stdint.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"

#include <DataStructures/tree.h>

typedef uintptr_t __uintptr_t;

namespace AVLTree {
    
    RB_GENERATE(raw_wAVLTree, wAVLTreeNode, rb_node, wAVLTree_compare)
    
    int wAVLTree_compare(wAVLTreeNode* lhs, wAVLTreeNode* rhs) {
    if (lhs->key < rhs->key)
        return -1;
    else if (lhs->key > rhs->key)
        return 1;
    return 0;
}

}

#pragma GCC diagnostic pop