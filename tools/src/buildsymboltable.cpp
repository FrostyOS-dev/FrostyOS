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

#include <stdio.h>
#include <string.h>

enum class Stage {
    Address,
    Space1,
    Ignore,
    Space2,
    Name,
    Done
};

int main(int argc, char** argv) {
    FILE* file = stdin;
    if (argc > 1) {
        if (strcmp(argv[1], "-") != 0) {
            file = fopen(argv[1], "r");
            if (file == NULL) {
                fprintf(stderr, "Error: Could not open file %s\n", argv[1]);
                return 1;
            }
        }
    }

    Stage stage = Stage::Address;

    char c = fgetc(file);
    while (c != EOF) {
        switch (stage) {
        case Stage::Address:
            if (c == ' ')
                stage = Stage::Space1;
            else if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))
                putchar(c);
            else {
                fprintf(stderr, "Error: Expected address or space, got %c\n", c);
                return 1;
            }
            break;
        case Stage::Space1:
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
                stage = Stage::Ignore;
            else {
                fprintf(stderr, "Error: Expected letter, got %c\n", c);
                return 1;
            }
            break;
        case Stage::Ignore:
            if (c == ' ')
                stage = Stage::Space2;
            else {
                fprintf(stderr, "Error: Expected space, got %c\n", c);
                return 1;
            }
            break;
        case Stage::Space2:
            stage = Stage::Name;
            printf(" %c", c);
            break;
        case Stage::Name:
            if (c == '\n')
                stage = Stage::Done;
            putchar(c);
            break;
        case Stage::Done:
            if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) {
                stage = Stage::Address;
                putchar(c);
            }
            else {
                fprintf(stderr, "Error: Expected address, got %c\n", c);
                return 1;
            }
            break;
        }
        c = fgetc(file);
    }
    return 0;
}