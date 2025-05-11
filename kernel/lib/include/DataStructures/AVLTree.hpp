#ifndef _AVLTREE_HPP
#define _AVLTREE_HPP

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#include "tree.h"
#pragma GCC diagnostic pop
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>

#ifndef _IS_IN_KERNEL
#define kcalloc_vmm calloc
#define kfree_vmm free
#endif

/*
This AVL tree implementation is based on the FreeBSD wAVL tree implementation.
The majority of the code for this is in the tree.h file in this directly, along with its license.
*/

namespace AVLTree {
    struct wAVLTreeNode {
        RB_ENTRY(wAVLTreeNode) rb_node;
        RB_ENTRY(wAVLTreeNode) pc_rb_node;
        uint64_t key;
        uint64_t value;
    };

    int wAVLTree_compare(wAVLTreeNode* lhs, wAVLTreeNode* rhs);

    typedef RB_HEAD(raw_wAVLTree, wAVLTreeNode) raw_wAVLTree;

    RB_PROTOTYPE(raw_wAVLTree, wAVLTreeNode, rb_node, wAVLTree_compare)

    template <typename K, typename D>
    class wAVLTree { // NOTE: No allocations of K (key) or D (data) are performed by this class. 
    public:
        wAVLTree() {
            RB_INIT(&m_tree);
        }

        void Insert(K key, D data) {
            wAVLTreeNode* node = (wAVLTreeNode*)kcalloc_vmm(1, sizeof(wAVLTreeNode));
            node->key = (uint64_t)key;
            node->value = (uint64_t)data;
            RB_INSERT(raw_wAVLTree, &m_tree, node);
        }

        void Remove(K key) {
            wAVLTreeNode node;
            node.key = (uint64_t)key;
            wAVLTreeNode* found_node = RB_FIND(raw_wAVLTree, &m_tree, &node);
            if (found_node != nullptr) {
                RB_REMOVE(raw_wAVLTree, &m_tree, found_node);
                kfree_vmm(found_node);
            }
        }

        D Find(K key) {
            wAVLTreeNode node;
            node.key = (uint64_t)key;
            wAVLTreeNode* found_node = RB_FIND(raw_wAVLTree, &m_tree, &node);
            if (found_node != nullptr) {
                return (D)found_node->value;
            }
            return nullptr;
        }

        wAVLTreeNode* FindNode(K key) {
            wAVLTreeNode node;
            node.key = (uint64_t)key;
            return RB_FIND(raw_wAVLTree, &m_tree, &node);
        }

        void forEach(void (*callback)(void*, K, D), void* data = nullptr) {
            wAVLTreeNode* node;
            RB_FOREACH(node, raw_wAVLTree, &m_tree) {
                callback(data, (K)node->key, (D)node->value);
            }
        }

        void forEach(bool (*callback)(void*, K, D), void* data = nullptr) {
            wAVLTreeNode* node;
            RB_FOREACH(node, raw_wAVLTree, &m_tree) {
                if (!callback(data, (K)node->key, (D)node->value))
                    break;
            }
        }

        ;
    private:
        raw_wAVLTree m_tree;
    };
}

#endif /* _AVLTREE_HPP */