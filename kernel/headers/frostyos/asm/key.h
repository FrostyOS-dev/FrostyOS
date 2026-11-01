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

#ifndef _FROSTYOS_ASM_KEY_H
#define _FROSTYOS_ASM_KEY_H

#include "types.h"

#define __FOSI_ENUMERATE_INPUT_KEYS(__FOSI_ENUM) \
    __FOSI_ENUM(ESC, "Escape") \
    __FOSI_ENUM(F1, "F1") \
    __FOSI_ENUM(F2, "F2") \
    __FOSI_ENUM(F3, "F3") \
    __FOSI_ENUM(F4, "F4") \
    __FOSI_ENUM(F5, "F5") \
    __FOSI_ENUM(F6, "F6") \
    __FOSI_ENUM(F7, "F7") \
    __FOSI_ENUM(F8, "F8") \
    __FOSI_ENUM(F9, "F9") \
    __FOSI_ENUM(F10, "F10") \
    __FOSI_ENUM(F11, "F11") \
    __FOSI_ENUM(F12, "F12") \
    __FOSI_ENUM(PRTSC, "Print Screen") \
    __FOSI_ENUM(SCRLK, "Scroll Lock") \
    __FOSI_ENUM(PAUSE, "Pause") \
    __FOSI_ENUM(BACKTICK, "`") \
    __FOSI_ENUM(ONE, "1") \
    __FOSI_ENUM(TWO, "2") \
    __FOSI_ENUM(THREE, "3") \
    __FOSI_ENUM(FOUR, "4") \
    __FOSI_ENUM(FIVE, "5") \
    __FOSI_ENUM(SIX, "6") \
    __FOSI_ENUM(SEVEN, "7") \
    __FOSI_ENUM(EIGHT, "8") \
    __FOSI_ENUM(NINE, "9") \
    __FOSI_ENUM(ZERO, "0") \
    __FOSI_ENUM(HYPHEN, "-") \
    __FOSI_ENUM(EQUAL, "=") \
    __FOSI_ENUM(BACKSPACE, "Backspace") \
    __FOSI_ENUM(INSERT, "Insert") \
    __FOSI_ENUM(HOME, "Home") \
    __FOSI_ENUM(PAGE_UP, "Page Up") \
    __FOSI_ENUM(NUM_LOCK, "Num Lock") \
    __FOSI_ENUM(NP_FORWARD_SLASH, "/") \
    __FOSI_ENUM(NP_ASTERISK, "*") \
    __FOSI_ENUM(NP_HYPHEN, "-") \
    __FOSI_ENUM(TAB, "Tab") \
    __FOSI_ENUM(Q, "Q") \
    __FOSI_ENUM(W, "W") \
    __FOSI_ENUM(E, "E") \
    __FOSI_ENUM(R, "R") \
    __FOSI_ENUM(T, "T") \
    __FOSI_ENUM(Y, "Y") \
    __FOSI_ENUM(U, "U") \
    __FOSI_ENUM(I, "I") \
    __FOSI_ENUM(O, "O") \
    __FOSI_ENUM(P, "P") \
    __FOSI_ENUM(OPEN_BRACKET, "[") \
    __FOSI_ENUM(CLOSE_BRACKET, "]") \
    __FOSI_ENUM(BACKSLASH, "\\") \
    __FOSI_ENUM(DELETE, "Delete") \
    __FOSI_ENUM(END, "End") \
    __FOSI_ENUM(PAGE_DOWN, "Page Down") \
    __FOSI_ENUM(NP_SEVEN, "7") \
    __FOSI_ENUM(NP_EIGHT, "8") \
    __FOSI_ENUM(NP_NINE, "9") \
    __FOSI_ENUM(NP_PLUS, "+") \
    __FOSI_ENUM(CAPS_LOCK, "Caps Lock") \
    __FOSI_ENUM(A, "A") \
    __FOSI_ENUM(S, "S") \
    __FOSI_ENUM(D, "D") \
    __FOSI_ENUM(F, "F") \
    __FOSI_ENUM(G, "G") \
    __FOSI_ENUM(H, "H") \
    __FOSI_ENUM(J, "J") \
    __FOSI_ENUM(K, "K") \
    __FOSI_ENUM(L, "L") \
    __FOSI_ENUM(SEMICOLON, ";") \
    __FOSI_ENUM(APOSTROPHE, "'") \
    __FOSI_ENUM(ENTER, "Enter") \
    __FOSI_ENUM(NP_FOUR, "4") \
    __FOSI_ENUM(NP_FIVE, "5") \
    __FOSI_ENUM(NP_SIX, "6") \
    __FOSI_ENUM(LEFT_SHIFT, "Left Shift") \
    __FOSI_ENUM(Z, "Z") \
    __FOSI_ENUM(X, "X") \
    __FOSI_ENUM(C, "C") \
    __FOSI_ENUM(V, "V") \
    __FOSI_ENUM(B, "B") \
    __FOSI_ENUM(N, "N") \
    __FOSI_ENUM(M, "M") \
    __FOSI_ENUM(COMMA, ",") \
    __FOSI_ENUM(DOT, ".") \
    __FOSI_ENUM(FORWARD_SLASH, "/") \
    __FOSI_ENUM(RIGHT_SHIFT, "Right Shift") \
    __FOSI_ENUM(UP_ARROW, "Up Arrow") \
    __FOSI_ENUM(NP_ONE, "1") \
    __FOSI_ENUM(NP_TWO, "2") \
    __FOSI_ENUM(NP_THREE, "3") \
    __FOSI_ENUM(NP_ENTER, "Enter") \
    __FOSI_ENUM(LEFT_CTRL, "Left Control") \
    __FOSI_ENUM(LEFT_SUPER, "Left Super") \
    __FOSI_ENUM(LEFT_ALT, "Left Alt") \
    __FOSI_ENUM(SPACE, "Space") \
    __FOSI_ENUM(RIGHT_ALT, "Right Alt") \
    __FOSI_ENUM(RIGHT_SUPER, "Right Super") \
    __FOSI_ENUM(CONTEXT_MENU, "Context Menu") \
    __FOSI_ENUM(RIGHT_CTRL, "Right Control") \
    __FOSI_ENUM(LEFT_ARROW, "Left Arrow") \
    __FOSI_ENUM(DOWN_ARROW, "Down Arrow") \
    __FOSI_ENUM(RIGHT_ARROW, "Right Arrow") \
    __FOSI_ENUM(NP_ZERO, "0") \
    __FOSI_ENUM(NP_DOT, ".") \
    __FOSI_ENUM(MM_PREVIOUS_TRACK, "Previous Track") \
    __FOSI_ENUM(MM_NEXT_TRACK, "Next Track") \
    __FOSI_ENUM(MM_MUTE, "Mute") \
    __FOSI_ENUM(MM_PLAY, "Play") \
    __FOSI_ENUM(MM_STOP, "Stop") \
    __FOSI_ENUM(MM_VOLUME_UP, "Volume Up") \
    __FOSI_ENUM(MM_VOLUME_DOWN, "Volume Down") \
    __FOSI_ENUM(MM_POWER, "Power") \
    __FOSI_ENUM(MM_SLEEP, "Sleep") \
    __FOSI_ENUM(MM_WAKE, "Wake") \
    __FOSI_ENUM(MM_BROWSER_HOME, "Browser Home") \
    __FOSI_ENUM(MM_BROWSER_SEARCH, "Browser Search") \
    __FOSI_ENUM(MM_BROWSER_FAVOURITES, "Browser Favourites") \
    __FOSI_ENUM(MM_BROWSER_REFRESH, "Browser Refresh") \
    __FOSI_ENUM(MM_BROWSER_STOP, "Browser Stop") \
    __FOSI_ENUM(MM_BROWSER_FORWARD, "Browser Forward") \
    __FOSI_ENUM(MM_BROWSER_BACK, "Browser Back") \
    __FOSI_ENUM(MM_MY_COMPUTER, "My Computer") \
    __FOSI_ENUM(MM_EMAIL, "Email") \
    __FOSI_ENUM(MM_MEDIA_SELECT, "Media Select") \
    __FOSI_ENUM(UNKNOWN, "Unknown")

enum FOSI_Key {
#define __FOSI_ENUMERATE_KEY(c, s) FOSI_KEY_##c,
    __FOSI_ENUMERATE_INPUT_KEYS(__FOSI_ENUMERATE_KEY)
#undef __FOSI_ENUMERATE_KEY
};

inline static const char* FOSI_GetKeyName(_u32 k) {
    switch (k) {
#define __FOSI_ENUMERATE_KEY(c, s) case FOSI_KEY_##c: return s;
        __FOSI_ENUMERATE_INPUT_KEYS(__FOSI_ENUMERATE_KEY)
#undef __FOSI_ENUMERATE_KEY
    default:
        return "Unknown";
    }
}

#endif /* _FROSTYOS_ASM_KEY_H */