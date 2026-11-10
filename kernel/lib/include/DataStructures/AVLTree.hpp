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

#include <Function.hpp>

#include <spinlock.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>

#include <Scheduling/Mutex.hpp>

/*
This AVL tree implementation is based on the FreeBSD wAVL tree implementation.
The majority of the code for this is in the tree.h file in this directory, along with its license.
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
        wAVLTree(bool vmm = false) : m_vmm(vmm), m_lock() {
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

        void forEach(FunctionRef<bool (K, D)> callback) {
            wAVLTreeNode* node;
            RB_FOREACH(node, raw_wAVLTree, &m_tree) {
                if (!callback((K)node->key, (D)node->value))
                    return;
            }
        }

        void forEach(FunctionRef<bool (K, D)> callback, K start) {
            wAVLTreeNode* startNode = FindNode(start);
            if (startNode == nullptr)
                return;
            wAVLTreeNode* node;
            RB_FOREACH_FROM(node, raw_wAVLTree, startNode) {
                if (!callback((K)node->key, (D)node->value))
                    return;
            }
        }

        void Clear() {
            wAVLTreeNode* next;
            for (wAVLTreeNode* node = RB_MIN(raw_wAVLTree, &m_tree); node != nullptr; node = next) {
                next = RB_NEXT(raw_wAVLTree, &m_tree, node);
                RB_REMOVE(raw_wAVLTree, &m_tree, node);
                if (m_vmm)
                    kfree_vmm(node);
                else
                    kfree(node);
            }
        }

        bool isEmpty() {
            return RB_EMPTY(&m_tree);
        }

        void lock() const {
            m_lock.Lock();
        }

        void unlock() const {
            m_lock.Unlock();
        }

    private:
        raw_wAVLTree m_tree;
        bool m_vmm;
        mutable Mutex m_lock;
    };


    struct Node {
        RB_ENTRY(Node) link;
        uint64_t key;

        void Reset() { link = {}; key = 0; }
    };

    int Node_compare(Node* a, Node* b);

    typedef RB_HEAD(raw_NodeTree, Node) raw_NodeTree;
    RB_PROTOTYPE(raw_NodeTree, Node, link, Node_compare)

    template <typename T, size_t NodeOffset>
    class IntrusiveTree {
        static_assert(NodeOffset + sizeof(Node) <= sizeof(T), "bad node offset");

        static Node* NodeOf(T* o) {
            return reinterpret_cast<Node*>(reinterpret_cast<char*>(o) + NodeOffset);
        }

        static T* ObjOf(Node* n) {
            return n != nullptr ? reinterpret_cast<T*>(reinterpret_cast<char*>(n) - NodeOffset) : nullptr;
        }

    public:
        IntrusiveTree() {
            RB_INIT(&m_tree);
        }
        IntrusiveTree(const IntrusiveTree&) = delete;
        IntrusiveTree& operator=(const IntrusiveTree&) = delete;

        bool Insert(T* obj, uint64_t key, T** existing = nullptr) {
            Node* n = NodeOf(obj);
            n->key = key;
            Node* dup = RB_INSERT(raw_NodeTree, &m_tree, n);
            if (dup != nullptr && existing != nullptr)
                *existing = ObjOf(dup);
            return dup == nullptr;
        }

        void Remove(T* obj) {
            RB_REMOVE(raw_NodeTree, &m_tree, NodeOf(obj));
        }

        T* Find(uint64_t key) {
            Node probe;
            probe.key = key;
            return ObjOf(RB_FIND(raw_NodeTree, &m_tree, &probe));
        }

        T* FindGE(uint64_t key) {
            Node probe;
            probe.key = key;
            return ObjOf(RB_NFIND(raw_NodeTree, &m_tree, &probe));
        }

        T* FindLE(uint64_t key) {
            Node probe;
            probe.key = key;
            Node* n = RB_NFIND(raw_NodeTree, &m_tree, &probe);
            if (n == nullptr)
                n = RB_MAX(raw_NodeTree, &m_tree);
            else if (n->key > key)
                n = RB_PREV(raw_NodeTree, &m_tree, n);
            return ObjOf(n);
        }

        T* First() {
            return ObjOf(RB_MIN(raw_NodeTree, &m_tree));
        }

        T* Last() {
            return ObjOf(RB_MAX(raw_NodeTree, &m_tree));
        }

        T* Next(T* o) {
            return ObjOf(RB_NEXT(raw_NodeTree, &m_tree, NodeOf(o)));
        }

        T* Prev(T* o) {
            return ObjOf(RB_PREV(raw_NodeTree, &m_tree, NodeOf(o)));
        }

        static uint64_t KeyOf(T* o) {
            return NodeOf(o)->key;
        }

        bool isEmpty() {
            return RB_EMPTY(&m_tree);
        }

        void ForEach(FunctionRef<bool (uint64_t, T*)> cb) {
            Node* n;
            RB_FOREACH(n, raw_NodeTree, &m_tree) {
                if (!cb(n->key, ObjOf(n)))
                    return;
            }
        }

        void ForEachFrom(FunctionRef<bool (uint64_t, T*)> cb, uint64_t startingKey) {
            Node n;
            n.key = startingKey;
            Node* start = RB_NFIND(raw_NodeTree, &m_tree, &n);
            Node* node = &n;
            RB_FOREACH_FROM(node, raw_NodeTree, start) {
                if (!cb(node->key, ObjOf(node)))
                    return;
            }
        }

        // Removal is allowed of the object provided to cb
        void ForEachFromSafe(FunctionRef<bool (uint64_t, T*)> cb, uint64_t startingKey) {
            Node probe;
            probe.key = startingKey;
            Node* n = RB_NFIND(raw_NodeTree, &m_tree, &probe);
            while (n != nullptr) {
                Node* next = RB_NEXT(raw_NodeTree, &m_tree, n);
                if (!cb(n->key, ObjOf(n)))
                    return;
                n = next;
            }
        }

        void Drain(FunctionRef<void (T*)> cb) {
            for (Node* n = RB_MIN(raw_NodeTree, &m_tree); n != nullptr; n = RB_MIN(raw_NodeTree, &m_tree)) {
                RB_REMOVE(raw_NodeTree, &m_tree, n);
                cb(ObjOf(n));
            }
        }

    private:
        raw_NodeTree m_tree;
    };
}

#endif /* _AVLTREE_HPP */