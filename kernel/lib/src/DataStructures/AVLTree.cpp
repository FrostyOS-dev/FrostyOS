/*
Copyright (©) 2025  Frosty515

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <stddef.h>
#include <stdint.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"

#include <DataStructures/AVLTree.hpp>
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

    int Node_compare(Node* a, Node* b) {
        return a->key < b->key ? -1 : (a->key > b->key ? 1 : 0);
    }

    RB_GENERATE(raw_NodeTree, Node, link, Node_compare);

}

#pragma GCC diagnostic pop
