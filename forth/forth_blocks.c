/*
* forth_blocks.c
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

// This file provides an implementation of the BLOCK word set.
// Many forth programmers cosider blocks to be obsolete, but on a small
// embedded system there may not be any file system.
// BLOCKS provide a means to access permanent storage and store source code or other that needs to be saved
// and retrieved later.
// Blocks are exacvtly 1KB (1024 bytes) in size.
// Here is the code for everything except to the actual physical read/write which needs to be provided by the
// system hosting the Forth subsystem.

#include <string.h>
#include <forth.h>
#include <forth_internal.h>

// BUFFER ( blk -- c-addr )
void forth_buffer(forth_runtime_context_t *ctx)
{
    forth_block_buffers_t *block_buffers = ctx->block_buffers;
    forth_scell_t blk = (forth_scell_t)forth_POP(ctx);
    forth_scell_t res;
    int ix, i;

    if (0 == block_buffers)
    {
        forth_THROW(ctx, -21);  // 	unsupported operation
    }

    if ((1 > blk) || ((FORTH_MAX_BLOCKS) < blk))
    {
        forth_THROW(ctx, -35); // invalid block number
    }

    for (i = 0; i < (FORTH_BLOCK_BUFFERS_COUNT); i++)
    {
        if (block_buffers->block_assigned[i] == blk)                        // A buffer is already assigned to the block.
        {
            block_buffers->last_used[i] = ++(block_buffers->age_clock);
            forth_PUSH(ctx, (forth_cell_t)(block_buffers->buffer[i]));
            block_buffers->current_buffer_index = i;
            return;
        }
    }

    ix = 0;
    for (i = 1; i < (FORTH_BLOCK_BUFFERS_COUNT); i++)
    {
        if (block_buffers->last_used[ix] > block_buffers->last_used[i])
        {
            ix = i;
        }
    }

    if (-1 == block_buffers->buffer_state[ix]) // Buffer is dirty
    {
        res = forth_WRITE_BLOCK(block_buffers->block_assigned[ix], block_buffers->buffer[ix]);

        if (0 != res)
        {
            forth_THROW(ctx, res);
        }
    }

    block_buffers->buffer_state[ix] = 0;    // Buffer is empty.
    block_buffers->last_used[ix] = ++(block_buffers->age_clock);
    block_buffers->block_assigned[ix] = blk;
    block_buffers->current_buffer_index = ix;
    res = forth_READ_BLOCK(blk, block_buffers->buffer[ix]);

    if (1 == res) // Resource was missing, e.g. file did not exists for a file based block.
    {
        memset(block_buffers->buffer[ix], FORTH_CHAR_SPACE, FORTH_BLOCK_BUFFER_SIZE);
    }
    else if (0 > res)
    {
        forth_THROW(ctx, res);
    }

    forth_PUSH(ctx, (forth_cell_t)(block_buffers->buffer[ix]));
}

// BLOCK ( blk -- c-addr )
void forth_block(forth_runtime_context_t *ctx)
{
    forth_scell_t res;
    forth_cell_t blk;
    char *buffer;

	forth_CHECK_STACK_AT_LEAST(ctx, 1);
    blk = ctx->sp[0];
    forth_buffer(ctx);

    if (0 == ctx->block_buffers->buffer_state[ctx->block_buffers->current_buffer_index])
    {
        buffer = (char *)(ctx->sp[0]);
        res = forth_READ_BLOCK(blk, buffer);

        if (1 == res) // Resource was missing, e.g. file did not exists for a file based block.
        {
            memset(buffer, FORTH_CHAR_SPACE, FORTH_BLOCK_BUFFER_SIZE);
        }
        else if (0 > res)
        {
            forth_THROW(ctx, res);
        }
        ctx->block_buffers->buffer_state[ctx->block_buffers->current_buffer_index] = 1;
    }
}

// LIST ( blk -- )
void forth_list(forth_runtime_context_t *ctx)
{
    int i;
    forth_cell_t scr;

	forth_CHECK_STACK_AT_LEAST(ctx, 1);

    scr = ctx->sp[0];
    forth_TYPE0(ctx, "SCR #");
    forth_UDOT(ctx, 10, scr);
    forth_cr(ctx);
    forth_block(ctx);

    for (i = 0; i < 16; i++)
    {
        forth_DOT_R(ctx, 10, i, 4, 0);
        forth_PUSH(ctx, 4);
        forth_spaces(ctx);
        forth_dup(ctx);
        forth_PUSH(ctx, 64);
        forth_type(ctx);
        forth_cr(ctx);
        ctx->sp[0] += 64;
    }

    forth_drop(ctx);
    ctx->block_buffers->scr = scr;
}

// UPDATE ( -- )
void forth_update(forth_runtime_context_t *ctx)
{
    if (0 == ctx->block_buffers)
    {
        forth_THROW(ctx, -21);  // 	unsupported operation
    }

    if ((0 <= ctx->block_buffers->current_buffer_index) || ((FORTH_BLOCK_BUFFERS_COUNT) > ctx->block_buffers->current_buffer_index))
    {
        ctx->block_buffers->buffer_state[ctx->block_buffers->current_buffer_index] = -1;
    } 
}

// SAVE-BUFFERS ( -- )
void forth_save_buffers(forth_runtime_context_t *ctx)
{
    forth_scell_t res;
    int i;

    if (0 == ctx->block_buffers)
    {
        forth_THROW(ctx, -21);  // 	unsupported operation
    }

    for (i = 0; i < (FORTH_BLOCK_BUFFERS_COUNT); i++)
    {
        if (-1 == ctx->block_buffers->buffer_state[i]) // Buffer is dirty
        {
            res = forth_WRITE_BLOCK(ctx->block_buffers->block_assigned[i], ctx->block_buffers->buffer[i]);

            if (0 != res)
            {
                forth_THROW(ctx, res);
            }
        }
    }
}

// EMPTY-BUFFERS ( -- )
void forth_empty_buffers(forth_runtime_context_t *ctx)
{
    int i;

    if (0 == ctx->block_buffers)
    {
        forth_THROW(ctx, -21);  // 	unsupported operation
    }

    memset(ctx->block_buffers, 0, sizeof(forth_block_buffers_t));
    ctx->block_buffers->buffer_state[ctx->block_buffers->current_buffer_index] = -1;
}

// FLUSH ( -- )
void forth_flush(forth_runtime_context_t *ctx)
{
    forth_save_buffers(ctx);
    forth_empty_buffers(ctx);
}

// BLK ( -- addr )
void forth_blk(forth_runtime_context_t *ctx)
{
    forth_PUSH(ctx, (forth_cell_t)&(ctx->blk));
}

// SCR ( -- addr )
void forth_scr(forth_runtime_context_t *ctx)
{
    forth_PUSH(ctx, (forth_cell_t)&(ctx->block_buffers->scr));
}

// If BLK has just been restored there is no expectation that the old buffer's address is still valid.
// So we need to readjust SOURCE's address (and length while we are at it, although it should be valid).
void forth_ADJUST_BLK_INPUT_SOURCE(forth_runtime_context_t *ctx, forth_cell_t blk)
{
    if (0 != blk)
    {
        forth_PUSH(ctx, blk);
        forth_block(ctx);
        ctx->source_address = (const char *)forth_POP(ctx);
        ctx->source_length = FORTH_BLOCK_BUFFER_SIZE;
    }
}

// LOAD ( i * x blk -- j * x )
void forth_load(forth_runtime_context_t *ctx)
{
	forth_scell_t res;
    forth_cell_t blk = forth_POP(ctx);
	forth_cell_t saved_blk = ctx->blk;
	forth_cell_t saved_in = ctx->to_in;
	const char *saved_source_address = ctx->source_address;
	forth_cell_t saved_source_length = ctx->source_length;

    forth_PUSH(ctx, blk);
    forth_block(ctx);
	ctx->blk = blk;
	ctx->source_address = (const char *)forth_POP(ctx);
	ctx->source_length = FORTH_BLOCK_BUFFER_SIZE;
	ctx->to_in = 0;

    res = forth_CATCH(ctx, forth_interpret_xt);

	ctx->source_address = saved_source_address;
    ctx->source_length = saved_source_length;
    ctx->blk = saved_blk;
	ctx->to_in = saved_in;

    if (0 != saved_blk)
    {
        forth_ADJUST_BLK_INPUT_SOURCE(ctx, saved_blk);
    }

	forth_THROW(ctx, res);
}

// THRU ( first_blk last_blk -- )
void forth_thru(forth_runtime_context_t *ctx)
{
    forth_cell_t first = forth_POP(ctx);
    forth_cell_t last  = forth_POP(ctx);
    forth_cell_t i;

    for (i = first; i<= last; i++)
    {
        forth_PUSH(ctx, i);
        forth_load(ctx);
    }
}

const forth_vocabulary_entry_t forth_wl_blocks[] =
{
DEF_FORTH_WORD( "list",  	 0, forth_list,      	    "( blk -- )"),
DEF_FORTH_WORD( "thru",  	 0, forth_thru,      	    "( first_blk last_blk -- )"),
DEF_FORTH_WORD( "load",  	 0, forth_load,      	    "( blk -- )"),
DEF_FORTH_WORD( "update",  	 0, forth_update,      	    "( -- )"),
DEF_FORTH_WORD( "save-buffers", 0, forth_save_buffers,  "( -- )"),
DEF_FORTH_WORD( "empty-buffers",0, forth_empty_buffers,  "( -- )"),
DEF_FORTH_WORD( "flush",     0, forth_flush,            "( -- )"),
DEF_FORTH_WORD( "block",     0, forth_block,      	    "( blk -- c-addr )"),
DEF_FORTH_WORD( "buffer",    0, forth_buffer,      	    "( blk -- c-addr )"),
DEF_FORTH_WORD( "scr",       0, forth_scr,              "( -- addr )"),
DEF_FORTH_WORD( "blk",       0, forth_blk,              "( -- addr )"),
DEF_FORTH_WORD(0, 0, 0, 0)
};
