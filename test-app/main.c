/*
* main.c
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

#include <stdio.h>
#include <forth.h>
#include "app.h"

forth_cell_t dictionary[DICTIONARY_SIZE];

int main()
{
    int res;
    puts("Hello World!");
    res = forth_run_forth_stdio(128, 128, "quit");
    //res = forth_run_forth_stdio(128, 128, ".( Hello World !) cr\r\n\\ Comment\r \\ Other comment \n 1 2 + . cr \r\n xxx");
    printf("result = %d\r\n", res);
    return 0;
}