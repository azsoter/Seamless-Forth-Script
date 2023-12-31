/*
* app.h
*
*  Created on: Sep 05, 2023
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
#include <alloca.h>
#include <string.h>
#include <forth.h>
#include <forth_internal.h>
#include "app.h"

static int write_str(struct forth_runtime_context *rctx, const char *str, forth_cell_t length)
{
	rctx->terminal_col += length;

	while(length--)
	{
		putchar(*str++);
	}

	fflush(stdout);
	return 0;
}

static int page(struct forth_runtime_context *rctx)
{
	rctx->terminal_col = 0;
	putchar(12);
	fflush(stdout);
	return 0;
}

static int send_cr(struct forth_runtime_context *rctx)
{
	puts("");
	fflush(stdout);
	rctx->terminal_col = 0;
	return 0;
}

static forth_scell_t accept_str(struct forth_runtime_context *rctx, char *buffer, forth_cell_t length)
{
	char *res;
	// This is not actually correct, but will do for now.
	res = fgets(buffer, length - 1, stdin);
	if (0 == res)
	{
		return -1;
	}
	else
	{
		return strlen(res);
	}
}

// These are fake, in reality they should be more complicated to follow the rules in the FORTH standard.
// KEY? and EKEY? cannot be implemented using stdio, they would need something like a low level terminal interface.
static forth_cell_t key(struct forth_runtime_context *rctx)
{
	char c = getchar();
	return (forth_cell_t)c;
}

static forth_cell_t key_q(struct forth_runtime_context *rctx)
{
	return 1;
}

static forth_cell_t ekey(struct forth_runtime_context *rctx)
{
	char c = getchar();
	return ((forth_cell_t)c) << 8;
}

static forth_cell_t ekey_q(struct forth_runtime_context *rctx)
{
	return 1;
}

static forth_cell_t ekey_to_char(struct forth_runtime_context *rctx, forth_cell_t ek)
{
	return ek >> 8;
}

forth_dictionary_t *dict = 0;

#if defined(FORTH_INCLUDE_BLOCKS)
forth_block_buffers_t block_buffers;
#endif

#define SEARCH_ORDER_SIZE 32 /* Perhaps move this to some header file at one point. */
// ------------------------------------------------------------------------------------------------
int forth_run_forth_stdio(unsigned int dstack_cells, unsigned int rstack_cells, const char *cmd)
{
	forth_scell_t res;
	struct forth_runtime_context *rctx;
	forth_cell_t *sp;
	forth_cell_t *rp;
	forth_cell_t *search_order;
	forth_context_init_data_t init_data = { 0 };
	forth_cell_t size = sizeof(struct forth_runtime_context) + sizeof(forth_cell_t) * (dstack_cells + rstack_cells + (SEARCH_ORDER_SIZE));

    char *ctx = alloca(size);

    if (0 == ctx)
    {
    	printf("ERROR: Failed to create Forth runtime context!\r\n");
    	return -1;
    }

    memset(ctx, 0, size);

    rctx = (struct forth_runtime_context *)ctx;
    sp = (forth_cell_t *)(rctx + 1);
    rp = sp + dstack_cells;
    search_order = rp + rstack_cells;

	init_data.data_stack = sp;
	init_data.data_stack_cell_count = dstack_cells;
	init_data.return_stack = rp;
	init_data.return_stack_cell_count = rstack_cells;

#if !defined(FORTH_WITHOUT_COMPILATION)
	if (0 == dict)
	{
		dict = Forth_InitDictionary(dictionary, sizeof(dictionary));
	}

	init_data.dictionary = dict;
	init_data.search_order = search_order;
	init_data.search_order_slots = (SEARCH_ORDER_SIZE);
#endif

	res = Forth_InitContext(rctx, &init_data);

	if (0 > res)
	{
		return (int)res;
	}

   	rctx->terminal_width = 80;
   	rctx->terminal_height = 25;
   	rctx->write_string = &write_str;
   	rctx->page = &page;
#if 0
   	rctx->at_xy = &at_xy;
#endif
   	rctx->send_cr = &send_cr;
    rctx->accept_string = &accept_str;
   	rctx->key = &key;
   	rctx->key_q = &key_q;
   	rctx->ekey = &ekey;
   	rctx->ekey_q = &ekey_q;
   	rctx->ekey_to_char = &ekey_to_char;

#if defined(FORTH_INCLUDE_BLOCKS)
	memset(&block_buffers, 0, sizeof(block_buffers));
	block_buffers.current_buffer_index = -1;
	rctx->block_buffers = &block_buffers;
#endif

    res = Forth(rctx, cmd, strlen(cmd), 1);

    return (int)res;
}
