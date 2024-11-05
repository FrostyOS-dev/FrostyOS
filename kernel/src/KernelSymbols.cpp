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

#include "KernelSymbols.hpp"

#include <spinlock.h>
#include <string.h>

SymbolTable::SymbolTable() : m_symbols(), m_lock(SPINLOCK_DEFAULT_VALUE) {

}

SymbolTable::~SymbolTable() {
    m_symbols.clear();
}

void SymbolTable::FillFromRawStringData(const char* data, size_t size) {
    /*
    Raw string data is formatted as each line being a symbol in the format:
    <address> <symbol name>
    */
    uint64_t current_address = 0;
    char const* current_name = nullptr;
    uint64_t current_name_length = 0;
    enum class Stage {
        Address,
        Name
    } stage = Stage::Address;
    for (size_t i = 0; i < size; i++) {
        if (stage == Stage::Address) {
            if (data[i] == ' ') {
                stage = Stage::Name;
                current_name = &data[i + 1];
                current_name_length = 0;
            }
            else if (data[i] >= '0' && data[i] <= '9') {
                current_address *= 16;
                current_address += data[i] - '0';
            }
            else if (data[i] >= 'a' && data[i] <= 'f') {
                current_address *= 16;
                current_address += data[i] - 'a' + 10;
            }
            else if (data[i] >= 'A' && data[i] <= 'F') {
                current_address *= 16;
                current_address += data[i] - 'A' + 10;
            }
        }
        else if (stage == Stage::Name) {
            if (data[i] == '\n') {
                char* name = new char[current_name_length + 1];
                memcpy((void*)name, current_name, current_name_length);
                name[current_name_length] = '\0';
                m_symbols.insert((void*)current_address, name);
                stage = Stage::Address;
                current_name = nullptr;
                current_name_length = 0;
                current_address = 0;
            }
            else
                current_name_length++;
        }
    }
}

const char* SymbolTable::FindSymbol(void* address, void** base_address) const {
    return m_symbols.findOrLower(address, base_address);
}

void SymbolTable::AddSymbol(void* address, const char* name) {
    m_symbols.insert(address, name);
}

void SymbolTable::RemoveSymbol(void* address) {
    m_symbols.remove(address);
}

void SymbolTable::Lock() {
    spinlock_acquire(&m_lock);
}

void SymbolTable::Unlock() {
    spinlock_release(&m_lock);
}

SymbolTable* g_KSymTable = nullptr;
