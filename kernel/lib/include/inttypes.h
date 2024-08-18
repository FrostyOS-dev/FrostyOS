/*
Copyright (©) 2023  Frosty515

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

#ifndef _INTTYPES_H
#define _INTTYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>


#define PRIdMAX "ld"
#define PRIiMAX "li"
#define PRIoMAX "lo"
#define PRIuMAX "lu"
#define PRIxMAX "lx"
#define PRIXMAX "lX"

#define PRId8 "d"
#define PRIi8 "i"
#define PRIo8 "o"
#define PRIu8 "u"
#define PRIx8 "x"
#define PRIX8 "X"

#define PRId16 "d"
#define PRIi16 "i"
#define PRIo16 "o"
#define PRIu16 "u"
#define PRIx16 "x"
#define PRIX16 "X"

#define PRId32 "d"
#define PRIi32 "i"
#define PRIo32 "o"
#define PRIu32 "u"
#define PRIx32 "x"
#define PRIX32 "X"

#define PRId64 "ld"
#define PRIi64 "li"
#define PRIo64 "lo"
#define PRIu64 "lu"
#define PRIx64 "lx"
#define PRIX64 "lX"

#define PRIdLEAST8 "d"
#define PRIiLEAST8 "i"
#define PRIoLEAST8 "o"
#define PRIuLEAST8 "u"
#define PRIxLEAST8 "x"
#define PRIXLEAST8 "X"

#define PRIdLEAST16 "d"
#define PRIiLEAST16 "i"
#define PRIoLEAST16 "o"
#define PRIuLEAST16 "u"
#define PRIxLEAST16 "x"
#define PRIXLEAST16 "X"

#define PRIdLEAST32 "d"
#define PRIiLEAST32 "i"
#define PRIoLEAST32 "o"
#define PRIuLEAST32 "u"
#define PRIxLEAST32 "x"
#define PRIXLEAST32 "X"

#define PRIdLEAST64 "ld"
#define PRIiLEAST64 "li"
#define PRIoLEAST64 "lo"
#define PRIuLEAST64 "lu"
#define PRIxLEAST64 "lx"
#define PRIXLEAST64 "lX"

#define PRIdFAST8 "d"
#define PRIiFAST8 "i"
#define PRIoFAST8 "o"
#define PRIuFAST8 "u"
#define PRIxFAST8 "x"
#define PRIXFAST8 "X"

#define PRIdFAST16 "d"
#define PRIiFAST16 "i"
#define PRIoFAST16 "o"
#define PRIuFAST16 "u"
#define PRIxFAST16 "x"
#define PRIXFAST16 "X"

#define PRIdFAST32 "d"
#define PRIiFAST32 "i"
#define PRIoFAST32 "o"
#define PRIuFAST32 "u"
#define PRIxFAST32 "x"
#define PRIXFAST32 "X"

#define PRIdFAST64 "ld"
#define PRIiFAST64 "li"
#define PRIoFAST64 "lo"
#define PRIuFAST64 "lu"
#define PRIxFAST64 "lx"
#define PRIXFAST64 "lX"

#define PRIdPTR "ld"
#define PRIiPTR "li"
#define PRIoPTR "lo"
#define PRIuPTR "lu"
#define PRIxPTR "lx"
#define PRIXPTR "lX"

#define SCNdMAX "ld"
#define SCNiMAX "li"
#define SCNoMAX "lo"
#define SCNuMAX "lu"
#define SCNxMAX "lx"

#define SCNd8 "d"
#define SCNi8 "i"
#define SCNo8 "o"
#define SCNu8 "u"
#define SCNx8 "x"

#define SCNd16 "d"
#define SCNi16 "i"
#define SCNo16 "o"
#define SCNu16 "u"
#define SCNx16 "x"

#define SCNd32 "d"
#define SCNi32 "i"
#define SCNo32 "o"
#define SCNu32 "u"
#define SCNx32 "x"

#define SCNd64 "ld"
#define SCNi64 "li"
#define SCNo64 "lo"
#define SCNu64 "lu"
#define SCNx64 "lx"

#define SCNdLEAST8 "d"
#define SCNiLEAST8 "i"
#define SCNoLEAST8 "o"
#define SCNuLEAST8 "u"
#define SCNxLEAST8 "x"

#define SCNdLEAST16 "d"
#define SCNiLEAST16 "i"
#define SCNoLEAST16 "o"
#define SCNuLEAST16 "u"
#define SCNxLEAST16 "x"

#define SCNdLEAST32 "d"
#define SCNiLEAST32 "i"
#define SCNoLEAST32 "o"
#define SCNuLEAST32 "u"
#define SCNxLEAST32 "x"

#define SCNdLEAST64 "ld"
#define SCNiLEAST64 "li"
#define SCNoLEAST64 "lo"
#define SCNuLEAST64 "lu"
#define SCNxLEAST64 "lx"

#define SCNdFAST8 "d"
#define SCNiFAST8 "i"
#define SCNoFAST8 "o"
#define SCNuFAST8 "u"
#define SCNxFAST8 "x"

#define SCNdFAST16 "d"
#define SCNiFAST16 "i"
#define SCNoFAST16 "o"
#define SCNuFAST16 "u"
#define SCNxFAST16 "x"

#define SCNdFAST32 "d"
#define SCNiFAST32 "i"
#define SCNoFAST32 "o"
#define SCNuFAST32 "u"
#define SCNxFAST32 "x"

#define SCNdFAST64 "ld"
#define SCNiFAST64 "li"
#define SCNoFAST64 "lo"
#define SCNuFAST64 "lu"
#define SCNxFAST64 "lx"

#define SCNdPTR "ld"
#define SCNiPTR "li"
#define SCNoPTR "lo"
#define SCNuPTR "lu"
#define SCNxPTR "lx"

typedef struct _imaxdiv_t {
    intmax_t quot;
    intmax_t rem;
} imaxdiv_t;

intmax_t imaxabs(intmax_t j);
imaxdiv_t imaxdiv(intmax_t numer, intmax_t denom);
intmax_t strtoimax(const char* nptr, char** endptr, int base);
uintmax_t strtoumax(const char* ptr, char** endptr, int base);

/*
intmax_t wcstoimax(const wchar_t* nptr, wchar_t** endptr, int base);
uintmax_t wcstoumax(const wchar_t* nptr, wchar_t** endptr, int base);
*/

#ifdef __cplusplus
}
#endif

#endif /* _INTTYPES_H */