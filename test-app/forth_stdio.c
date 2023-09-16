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

#define SEARCH_ORDER_SIZE 32 /* Perhaps move this to some header file at one point. */


int forth_run_forth_stdio(unsigned int dstack_cells, unsigned int rstack_cells, const char *cmd)
{
	struct forth_runtime_context *rctx;
	forth_cell_t *sp;
	forth_cell_t *rp;
	forth_cell_t *search_order;
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
#if 0
    search_order = rp + rstack_cells;
#endif
#if 0
    rctx->dictionary = dictionary;
#endif
    rctx->sp0 = rp - 1;
    rctx->sp = rctx->sp0;
    rctx->sp_max = rctx->sp0;
   	rctx->sp_min = sp;

   	rctx->rp0 = (rp + rstack_cells) - 1;
   	rctx->rp = rctx->rp0;
   	rctx->rp_max = rctx->rp0;
   	rctx->rp_min = rp;

   	rctx->throw_handler = 0;
#if 0
   	rctx->ip = 0;
#endif
    rctx->base = 10;

#if 0
    rctx->wordlists = search_order;
   	rctx->wordlist_slots = (SEARCH_ORDER_SIZE);
   	rctx->wordlist_cnt = 2;
   	search_order[(SEARCH_ORDER_SIZE) - 1] = FORTH_WID_Root_WORDLIST;
   	search_order[(SEARCH_ORDER_SIZE) - 2] = FORTH_WID_FORTH_WORDLIST;
    rctx->current = FORTH_WID_FORTH_WORDLIST;
    // r_ctx.wordlist_cnt =
#endif
    		
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
#if 0
   	rctx->extra.fields.telnet.sd = sd;
   	rctx->extra.fields.telnet.latest_key = -1;



	rctx->extra.selector = FORTH_FEATURE_SELECTOR_TELNET;
	rctx->extra.caller_context = 0;
#endif
    forth(rctx, cmd);

//   	free(ctx);
    return 0;
}
// ------------------------------------------------------------------------------------------------
