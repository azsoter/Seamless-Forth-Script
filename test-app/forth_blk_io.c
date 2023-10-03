/*
* forth_blk_io.c
*
*  Created on: Oct 02, 2023
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

#include <stdio.h>
#include <forth.h>
#include <forth_internal.h>

// If fopen() fails this functions returns 1 indicating the the file (probably) does not exist.
forth_scell_t forth_READ_BLOCK(forth_cell_t block_number, uint8_t *buffer)
{
    forth_scell_t res = 0;
    FILE *f;
    char file_name[64];
    sprintf(file_name,"blk/%08x.blk", (unsigned int)block_number);
    f = fopen(file_name, "rb");
    if (0 == f)
    {
        puts("Could not open block file for reading.");
        return 1;   // We return 1 to indicated that the file does not exist.
    }

    if (1 != fread(buffer, 1024, 1 , f))
    {
        puts("fread() has failed!");
        res = -33; // Block read exception.
    }

    (void)fclose(f);

    return res;
}

forth_scell_t forth_WRITE_BLOCK(forth_cell_t block_number, uint8_t *buffer)
{
    forth_scell_t res = 0;
    FILE *f;
    char file_name[64];
    sprintf(file_name,"blk/%08x.blk", (unsigned int)block_number);
    f = fopen(file_name, "wb");

    if (0 == f)
    {
        puts("fopen() has failed!");
        return -34; // Block write exception.
    }

    if (1 != fwrite(buffer, 1024, 1 , f))
    {
        puts("fwrite() has failed!");
        res = -34; // Block write exception.
    }

    if (0 != fclose(f))
    {
        puts("fclose() has failed!");
        res = -34; // Block write exception.
    }

    return res;
}
