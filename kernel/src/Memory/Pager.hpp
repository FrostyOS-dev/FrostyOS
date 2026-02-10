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

#ifndef _VMM_PAGER_HPP
#define _VMM_PAGER_HPP

namespace VMM {
    // physical memory backed pager, subclasses can expand it to be more
    class DefaultPager {
    public:
        DefaultPager();
        virtual ~DefaultPager();

        virtual void* AllocatePage();
        virtual void FreePage(void* page);

    };

    extern DefaultPager* g_defaultPager;
}

#endif /* _VMM_PAGER_HPP */