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

#ifndef _WM_RECT_HPP
#define _WM_RECT_HPP

#include <algorithm>

namespace WM {
    struct Rect {
        int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        bool Empty() const {
            return x1 <= x0 || y1 <= y0;
        }
        int Width() const { return x1 - x0; }
        int Height() const { return y1 - y0; }
        Rect Interset(const Rect& o) const {
            return {std::max(x0, o.x0), std::max(y0, o.y0), std::min(x1, o.x1), std::min(y1, o.y1)};
        }
        void Add(const Rect& o) {
            if (o.Empty())
                return;
            if (Empty()) {
                *this = o;
                return;
            }
            x0 = std::min(x0, o.x0);
            x1 = std::max(x1, o.x1);
            y0 = std::min(y0, o.y0);
            y1 = std::max(y1, o.y1);
        }
        Rect Moved(int dx, int dy) const {
            return {x0 + dx, y0 + dy, x1 + dx, y1 + dy};
        }
    };
}

#endif /* _WM_RECT_HPP */