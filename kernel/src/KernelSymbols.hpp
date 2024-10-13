/*
Copyright (©) 2024  Frosty515

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

#ifndef _KERNEL_SYMBOLS_HPP
#define _KERNEL_SYMBOLS_HPP

#include <spinlock.h>
#include <stddef.h>

#include <Data-structures/AVLTree.hpp>

// Currently only supports a single range of symbols. There must not be any gaps or overlap.
class SymbolTable {
public:
    SymbolTable();
    ~SymbolTable();

    void FillFromRawStringData(const char* data, size_t size);

    const char* FindSymbol(void* address, void** base_address = nullptr) const;

    void AddSymbol(void* address, const char* name);
    void RemoveSymbol(void* address);

    void Lock();
    void Unlock();

private:
    AVLTree::SimpleAVLTree<void*, const char*> m_symbols;
    spinlock_t m_lock;
};

extern SymbolTable* g_KSymTable;

#endif /* _KERNEL_SYMBOLS_HPP */