/*
Copyright (©) 2026  Frosty515

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

#ifndef _HASHMAP_HPP
#define _HASHMAP_HPP

#include "AVLTree.hpp"

template <typename K, typename D>
class HashMap { // No allocation of K or D is performed by this class, essentially just a wrapper around the AVLTree::wAVLTree class
public:
    HashMap() {
        
    }

    void insert(K key, D data) {
        m_tree.Insert(key, data);
    }

    void remove(K key) {
        m_tree.Remove(key);
    }

    D get(K key) {
        return m_tree.Find(key);
    }

    void forEach(void (*callback)(void*, K, D), void* data = nullptr) {
        m_tree.forEach(callback, data);
    }

    void forEach(bool (*callback)(void*, K, D), void* data = nullptr) {
        m_tree.forEach(callback, data);
    }

    void lock() {
        m_tree.lock();
    }

    void unlock() {
        m_tree.unlock();
    }

private:
    AVLTree::wAVLTree<K, D> m_tree;
};

#endif /* _HASHMAP_HPP */