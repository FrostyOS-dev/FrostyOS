/*
Copyright (©) 2025-2026  Frosty515

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

#ifndef _AVLTREE_HPP
#define _AVLTREE_HPP

#include <cassert>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#include "tree.h"
#pragma GCC diagnostic pop

#include <spinlock.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>

/*
This AVL tree implementation is based on the FreeBSD wAVL tree implementation.
The majority of the code for this is in the tree.h file in this directly, along with its license.
*/

namespace AVLTree {
    struct wAVLTreeNode {
        RB_ENTRY(wAVLTreeNode) rb_node;
        uint64_t key;
        uint64_t value;
    };

    int wAVLTree_compare(wAVLTreeNode* lhs, wAVLTreeNode* rhs);

    typedef RB_HEAD(raw_wAVLTree, wAVLTreeNode) raw_wAVLTree;

    RB_PROTOTYPE(raw_wAVLTree, wAVLTreeNode, rb_node, wAVLTree_compare)

    template <typename K, typename D>
    class wAVLTree { // NOTE: No allocations of K (key) or D (data) are performed by this class. 
    public:
        wAVLTree(bool vmm = false) : m_vmm(vmm), m_lock(SPINLOCK_DEFAULT_VALUE) {
            RB_INIT(&m_tree);
        }

        wAVLTreeNode* Insert(K key, D data) {
            wAVLTreeNode* node;
            if (m_vmm)
                node = (wAVLTreeNode*)kcalloc_vmm(1, sizeof(wAVLTreeNode));
            else
                node = (wAVLTreeNode*)kcalloc(1, sizeof(wAVLTreeNode));
            node->key = (uint64_t)key;
            node->value = (uint64_t)data;
            RB_INSERT(raw_wAVLTree, &m_tree, node);

            return node;
        }

        void Remove(K key) {
            wAVLTreeNode node;
            node.key = (uint64_t)key;
            wAVLTreeNode* found_node = RB_FIND(raw_wAVLTree, &m_tree, &node);
            if (found_node != nullptr) {
                RB_REMOVE(raw_wAVLTree, &m_tree, found_node);
                if (m_vmm)
                    kfree_vmm(found_node);
                else
                    kfree(found_node);
            }
        }

        void RemoveNode(wAVLTreeNode* node) {
            if (node != nullptr) {
                RB_REMOVE(raw_wAVLTree, &m_tree, node);
                if (m_vmm)
                    kfree_vmm(node);
                else
                    kfree(node);
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

        wAVLTreeNode* FindNodeOrHigher(K key) {
            wAVLTreeNode node;
            node.key = (uint64_t)key;
            return RB_NFIND(raw_wAVLTree, &m_tree, &node);
        }

        wAVLTreeNode* FindNodeOrLower(K key) {
            wAVLTreeNode node;
            node.key = (uint64_t)key;
            wAVLTreeNode* new_node = RB_NFIND(raw_wAVLTree, &m_tree, &node);
            if (new_node == nullptr)
                return RB_MAX(raw_wAVLTree, &m_tree);
            if (new_node->key > (uint64_t)key)
                return RB_PREV(raw_wAVLTree, &m_tree, new_node);
            return new_node;
        }

        wAVLTreeNode* PreviousNode(wAVLTreeNode* node) const {
            return RB_PREV(raw_wAVLTree, &m_tree, node);
        }

        wAVLTreeNode* NextNode(wAVLTreeNode* node) const {
            return RB_NEXT(raw_wAVLTree, &m_tree, node);
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

        void forEach(void (*callback)(void*, wAVLTreeNode*), void* data = nullptr) {
            wAVLTreeNode* node;
            RB_FOREACH(node, raw_wAVLTree, &m_tree) {
                callback(data, node);
            }
        }

        void lock() const {
            spinlock_acquire(&m_lock);
        }

        void unlock() const {
            spinlock_release(&m_lock);
        }

    private:
        raw_wAVLTree m_tree;
        bool m_vmm;
        mutable spinlock_t m_lock;
    };
}

#endif /* _AVLTREE_HPP */