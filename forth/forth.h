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

#ifdef __cplusplus
extern "C" {
#endif

#include <forth_config.h>

#define FORTH_ENGINE_VERSION 5

// Some of the structs are defined in forth_internals.h because their structure is not part of the interface
// and only the Forth implementation (but not code calling it) needs to know about the details.

typedef struct forth_runtime_context forth_runtime_context_t;
typedef void (*forth_behavior_t)(forth_runtime_context_t *ctx);
typedef struct forth_dictionary forth_dictionary_t;

struct forth_context_init_data
{
    forth_dictionary_t *dictionary;         // An already initialized Forth dictionary.
    forth_cell_t *data_stack;               // The lowest address in the data stack area. 
    forth_cell_t *return_stack;             // The highest address in the data stack area. 
    forth_cell_t data_stack_cell_count;     // The size of the data stack in cells.
    forth_cell_t return_stack_cell_count;   // The size of the data stack in cells.
    forth_cell_t *search_order;             // The address of the cells to be used for search order.
    forth_cell_t search_order_slots;        // The number of cells in the search order area.
};
typedef struct forth_context_init_data forth_context_init_data_t;

extern forth_cell_t Forth_GetContextSize(void);
extern forth_scell_t Forth_InitContext(forth_runtime_context_t *ctx, const forth_context_init_data_t *init_data);
extern forth_dictionary_t *Forth_InitDictionary(void *addr, forth_cell_t length);
extern forth_scell_t Forth_Try(forth_runtime_context_t *ctx, forth_behavior_t f, char *name);
extern forth_scell_t Forth(forth_runtime_context_t *ctx, const char *cmd, unsigned int cmd_length, int clear_stack);

#ifdef __cplusplus
}
#endif

#endif
