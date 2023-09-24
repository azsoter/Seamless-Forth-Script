#ifndef FORTH_CONFIG_DEFAULT_H
#define FORTH_CONFIG_DEFAULT_H
/*
* forth_config.h
*
*  Created on: Jan 30, 2023
*      Author: Andras Zsoter
*
* Copyright (c) 2023 Andras Zsoter
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*
*/

#include <stdint.h>

#if defined(FORTH_IS_64BIT)

typedef int64_t  forth_scell_t;
typedef uint64_t forth_ucell_t;

#if defined(__GNUC__)
typedef unsigned __int128 forth_udcell_t;
typedef __int128  forth_sdcell_t;
#endif

typedef forth_udcell_t forth_dcell_t;

#define	FORTH_CELL_LOW(X) ((forth_cell_t)((~(forth_cell_t)0) & (X)))
#define	FORTH_CELL_HIGH(X) ((forth_cell_t)((~(forth_cell_t)0) & ((X)>>64)))
#define FORTH_DCELL(H, L)  ((((forth_dcell_t)(H)) << 64) + (L))

#define FORTH_CELL_HEX_DIGITS 16

#elif defined(FORTH_IS_32BIT)


typedef int32_t  forth_scell_t;
typedef uint32_t forth_ucell_t;
typedef uint64_t forth_udcell_t;
typedef int64_t  forth_sdcell_t;

typedef forth_udcell_t forth_dcell_t;

#define FORTH_CELL_HEX_DIGITS 8

#define	FORTH_CELL_LOW(X) ((forth_cell_t)(0xFFFFFFFF & (X)))
#define	FORTH_CELL_HIGH(X) ((forth_cell_t)(0xFFFFFFFF & ((X)>> 32)))
#define FORTH_DCELL(H, L)  ((((forth_dcell_t)(H)) << 32) + (L))
#else
#error Only 32 and 64 bit systems are supported.
#endif

typedef uint8_t	forth_byte_t;
typedef forth_ucell_t forth_cell_t;

// Works if sizeof(forth_cell_t) is a power of 2.
#define FORTH_ALIGN(X) ((((forth_ucell_t)(X)) + (sizeof(forth_ucell_t) - 1)) & ~(sizeof(forth_ucell_t) - 1))
#define FORTH_ALIGNED_MASK (~(sizeof(forth_ucell_t) - 1))

#define FORTH_NUM_BUFF_LENGTH (128 + 4)

#endif
