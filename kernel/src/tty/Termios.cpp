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

#include "Termios.hpp"

#define BAUD_CASE(x) case B##x: return x;

int termios_BaudToNumber(termios_t* termios) {
    switch (termios->c_cflag & CBAUD) {
        BAUD_CASE(0)
        BAUD_CASE(50)
        BAUD_CASE(75)
        BAUD_CASE(110)
        BAUD_CASE(134)
        BAUD_CASE(150)
        BAUD_CASE(200)
        BAUD_CASE(300)
        BAUD_CASE(600)
        BAUD_CASE(1200)
        BAUD_CASE(1800)
        BAUD_CASE(2400)
        BAUD_CASE(4800)
        BAUD_CASE(9600)
        BAUD_CASE(19200)
        BAUD_CASE(38400)
        BAUD_CASE(57600)
        BAUD_CASE(115200)
        BAUD_CASE(230400)
        BAUD_CASE(460800)
        BAUD_CASE(500000)
        BAUD_CASE(576000)
        BAUD_CASE(921600)
        BAUD_CASE(1000000)
        BAUD_CASE(1152000)
        BAUD_CASE(1500000)
        BAUD_CASE(2000000)
        BAUD_CASE(2500000)
        BAUD_CASE(3000000)
        BAUD_CASE(3500000)
        BAUD_CASE(4000000)
    }

    return -1;
}
