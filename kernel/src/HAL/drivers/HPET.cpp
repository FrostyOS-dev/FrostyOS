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

#include "HPET.hpp"

#include <assert.h>
#include <stdint.h>
#include <util.h>

#include <Memory/PageMapper.hpp>
#include <Memory/PagingUtil.hpp>

HPET* g_HPET = nullptr;

HPET::HPET(uint64_t address) : m_address(to_HHDM(address)), m_period(0), m_nsperiod(0), m_freq(0) {

}

HPET::~HPET() {

}

bool HPET::Init() {
    if (!g_KPageMapper->MapPage(m_address, from_HHDM(m_address), VMM::Protection::READ_WRITE))
        return false;

    uint64_t data = volatile_addr_read64(m_address + static_cast<uint64_t>(HPET_Register::CAP_ID));
    HPET_GeneralCapID* CAPID = reinterpret_cast<HPET_GeneralCapID*>(&data);
    m_period = CAPID->COUNTER_CLK_PERIOD;
    m_nsperiod = m_period / 1'000'000;
    assert(m_nsperiod > 0);
    m_freq = 1'000'000'000'000'000UL / m_period;

    data = volatile_addr_read64(m_address + static_cast<uint64_t>(HPET_Register::CONFIG));
    data &= ~2; // Clear LEG_RT_CNF
    data |= 1; // Set ENABLE_CNF
    volatile_addr_write64(m_address + static_cast<uint64_t>(HPET_Register::CONFIG), data);

    return true;
}

uint64_t HPET::GetNSTicks() {
    return m_nsperiod * volatile_addr_read64(m_address + static_cast<uint64_t>(HPET_Register::MAIN_COUNTER));
}
