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

#ifndef _HAL_INPUT_KEY_HPP
#define _HAL_INPUT_KEY_HPP

#define ENUMERATE_INPUT_KEYS(ENUM) \
    ENUM(ESC, "Escape") \
    ENUM(F1, "F1") \
    ENUM(F2, "F2") \
    ENUM(F3, "F3") \
    ENUM(F4, "F4") \
    ENUM(F5, "F5") \
    ENUM(F6, "F6") \
    ENUM(F7, "F7") \
    ENUM(F8, "F8") \
    ENUM(F9, "F9") \
    ENUM(F10, "F10") \
    ENUM(F11, "F11") \
    ENUM(F12, "F12") \
    ENUM(PRTSC, "Print Screen") \
    ENUM(SCRLK, "Scroll Lock") \
    ENUM(PAUSE, "Pause") \
    ENUM(BACKTICK, "`") \
    ENUM(ONE, "1") \
    ENUM(TWO, "2") \
    ENUM(THREE, "3") \
    ENUM(FOUR, "4") \
    ENUM(FIVE, "5") \
    ENUM(SIX, "6") \
    ENUM(SEVEN, "7") \
    ENUM(EIGHT, "8") \
    ENUM(NINE, "9") \
    ENUM(ZERO, "0") \
    ENUM(HYPHEN, "-") \
    ENUM(EQUAL, "=") \
    ENUM(BACKSPACE, "Backspace") \
    ENUM(INSERT, "Insert") \
    ENUM(HOME, "Home") \
    ENUM(PAGE_UP, "Page Up") \
    ENUM(NUM_LOCK, "Num Lock") \
    ENUM(NP_FORWARD_SLASH, "/") \
    ENUM(NP_ASTERISK, "*") \
    ENUM(NP_HYPHEN, "-") \
    ENUM(TAB, "Tab") \
    ENUM(Q, "Q") \
    ENUM(W, "W") \
    ENUM(E, "E") \
    ENUM(R, "R") \
    ENUM(T, "T") \
    ENUM(Y, "Y") \
    ENUM(U, "U") \
    ENUM(I, "I") \
    ENUM(O, "O") \
    ENUM(P, "P") \
    ENUM(OPEN_BRACKET, "[") \
    ENUM(CLOSE_BRACKET, "]") \
    ENUM(BACKSLASH, "\\") \
    ENUM(DELETE, "Delete") \
    ENUM(END, "End") \
    ENUM(PAGE_DOWN, "Page Down") \
    ENUM(NP_SEVEN, "7") \
    ENUM(NP_EIGHT, "8") \
    ENUM(NP_NINE, "9") \
    ENUM(NP_PLUS, "+") \
    ENUM(CAPS_LOCK, "Caps Lock") \
    ENUM(A, "A") \
    ENUM(S, "S") \
    ENUM(D, "D") \
    ENUM(F, "F") \
    ENUM(G, "G") \
    ENUM(H, "H") \
    ENUM(J, "J") \
    ENUM(K, "K") \
    ENUM(L, "L") \
    ENUM(SEMICOLON, ";") \
    ENUM(APOSTROPHE, "'") \
    ENUM(ENTER, "Enter") \
    ENUM(NP_FOUR, "4") \
    ENUM(NP_FIVE, "5") \
    ENUM(NP_SIX, "6") \
    ENUM(LEFT_SHIFT, "Left Shift") \
    ENUM(Z, "Z") \
    ENUM(X, "X") \
    ENUM(C, "C") \
    ENUM(V, "V") \
    ENUM(B, "B") \
    ENUM(N, "N") \
    ENUM(M, "M") \
    ENUM(COMMA, ",") \
    ENUM(DOT, ".") \
    ENUM(FORWARD_SLASH, "/") \
    ENUM(RIGHT_SHIFT, "Right Shift") \
    ENUM(UP_ARROW, "Up Arrow") \
    ENUM(NP_ONE, "1") \
    ENUM(NP_TWO, "2") \
    ENUM(NP_THREE, "3") \
    ENUM(NP_ENTER, "Enter") \
    ENUM(LEFT_CTRL, "Left Control") \
    ENUM(LEFT_SUPER, "Left Super") \
    ENUM(LEFT_ALT, "Left Alt") \
    ENUM(SPACE, "Space") \
    ENUM(RIGHT_ALT, "Right Alt") \
    ENUM(RIGHT_SUPER, "Right Super") \
    ENUM(CONTEXT_MENU, "Context Menu") \
    ENUM(RIGHT_CTRL, "Right Control") \
    ENUM(LEFT_ARROW, "Left Arrow") \
    ENUM(DOWN_ARROW, "Down Arrow") \
    ENUM(RIGHT_ARROW, "Right Arrow") \
    ENUM(NP_ZERO, "0") \
    ENUM(NP_DOT, ".") \
    ENUM(MM_PREVIOUS_TRACK, "Previous Track") \
    ENUM(MM_NEXT_TRACK, "Next Track") \
    ENUM(MM_MUTE, "Mute") \
    ENUM(MM_PLAY, "Play") \
    ENUM(MM_STOP, "Stop") \
    ENUM(MM_VOLUME_UP, "Volume Up") \
    ENUM(MM_VOLUME_DOWN, "Volume Down") \
    ENUM(MM_POWER, "Power") \
    ENUM(MM_SLEEP, "Sleep") \
    ENUM(MM_WAKE, "Wake") \
    ENUM(MM_BROWSER_HOME, "Browser Home") \
    ENUM(MM_BROWSER_SEARCH, "Browser Search") \
    ENUM(MM_BROWSER_FAVOURITES, "Browser Favourites") \
    ENUM(MM_BROWSER_REFRESH, "Browser Refresh") \
    ENUM(MM_BROWSER_STOP, "Browser Stop") \
    ENUM(MM_BROWSER_FORWARD, "Browser Forward") \
    ENUM(MM_BROWSER_BACK, "Browser Back") \
    ENUM(MM_MY_COMPUTER, "My Computer") \
    ENUM(MM_EMAIL, "Email") \
    ENUM(MM_MEDIA_SELECT, "Media Select") \
    ENUM(UNKNOWN, "Unknown")

enum class Key {
#define ENUMERATE_KEY(c, s) c,
    ENUMERATE_INPUT_KEYS(ENUMERATE_KEY)
#undef ENUMERATE_KEY
};

inline constexpr const char* GetKeyName(Key k) {
    switch (k) {
#define ENUMERATE_KEY(c, s) case Key::c: return s;
        ENUMERATE_INPUT_KEYS(ENUMERATE_KEY)
#undef ENUMERATE_KEY
    default:
        return "Unknown";
    }
}

#endif /* _HAL_INPUT_KEY_HPP */