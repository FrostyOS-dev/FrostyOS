/*
Copyright (©) 2024-2026  Frosty515

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

#include <DataStructures/AVLTree.hpp>

// Currently only supports a single range of symbols. There must not be any gaps or overlap.
class SymbolTable {
public:
    SymbolTable();
    ~SymbolTable();

    void FillFromRawStringData(const char* data, size_t size);

    const char* FindSymbol(void* address, void** base_address = nullptr);

    void AddSymbol(void* address, const char* name);
    void RemoveSymbol(void* address);

    void SetMemRegion(const void* regionStart, const void* regionEnd);

    void Lock();
    void Unlock();

private:
    AVLTree::wAVLTree<void*, const char*> m_symbols;
    void const* m_regionStart;
    void const* m_regionEnd;
    spinlock_t m_lock;
};

extern SymbolTable* g_KSymTable;

extern "C" {
    extern const void* _kernel_start_addr;
    extern const void* _kernel_end_addr;
}

#endif /* _KERNEL_SYMBOLS_HPP */