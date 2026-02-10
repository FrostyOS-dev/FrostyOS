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

#include "Pager.hpp"
#include "PMM.hpp"

namespace VMM {
    DefaultPager::DefaultPager() {

    }

    DefaultPager::~DefaultPager() {

    }

    void* DefaultPager::AllocatePage() {
        return g_PMM->AllocatePage();
    }

    void DefaultPager::FreePage(void* page) {
        g_PMM->FreePage(page);
    }

    DefaultPager* g_defaultPager = nullptr;
}