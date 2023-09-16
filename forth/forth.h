#ifndef FORTH_H
#define FORTH_H
/*
* forth.h
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

#include <forth_config.h>

// For implementing well-formed flags in Forth
#define FORTH_FALSE ((forth_ucell_t)0)
#define FORTH_TRUE  (~(forth_ucell_t)0)
#define FORTH_CHAR_SPACE 0x20


typedef struct forth_word_header_struct forth_word_header;



typedef struct forth_runtime_context forth_runtime_context_t;

typedef void (*forth_behavior)(forth_runtime_context_t *ctx);

typedef struct forth_vocabulary_entry_struct forth_vocabulary_entry_t;
typedef struct forth_runtime_context forth_runtime_context_t;

//extern int forth_execute_xt(forth_runtime_context_t *ctx, forth_xt_t xt);
//extern int forth_evaluate(forth_runtime_context_t *ctx, const char *cmd);

extern int forth(forth_runtime_context_t *ctx, const char *cmd);

#endif