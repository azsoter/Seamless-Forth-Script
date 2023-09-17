#ifndef FORTH_INTERNAL_H
#define FORTH_INTERNAL_H

/*
* forth_internal.h
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
#include <forth.h>
#include <setjmp.h>

struct forth_vocabulary_entry_struct
{
    forth_cell_t name;
	forth_ucell_t flags;
	forth_cell_t link;
    forth_ucell_t meaning;
};

#if defined(FORTH_EXCLUDE_DESCRIPTIONS)
#define DEF_FORTH_WORD(N, F, M, D)	{ (forth_cell_t)N, F, (forth_ucell_t)0, (forth_ucell_t)M }
#else
#define DEF_FORTH_WORD(N, F, M, D)	{ (forth_cell_t)N, F, (forth_ucell_t)D, (forth_ucell_t)M }
#endif

struct forth_runtime_context
{
	forth_cell_t	*dictionary;
	forth_cell_t	*sp_max;
	forth_cell_t	*sp_min;
	forth_cell_t	*sp0;
	forth_cell_t	*sp;
	forth_cell_t	*rp_max;
	forth_cell_t	*rp_min;
	forth_cell_t	*rp0;
	forth_cell_t	*rp;
//	forth_index_t	ip;
	forth_cell_t	base;	// Numeric base.
	forth_cell_t	state;
	forth_cell_t	throw_handler;
	forth_cell_t	bye_handler;
	forth_cell_t	quit_handler;
	forth_cell_t	abort_msg_len;		// Used by ABORT"
	forth_cell_t	abort_msg_addr;		// Used by ABORT"
	forth_cell_t	blk;		// BLK
	forth_cell_t	source_id;
	const char		*source_address;
	forth_cell_t	source_length;
	forth_cell_t	to_in;
#if defined(FORTH_INCLUDE_FILE_ACCESS_WORDS)
	forth_dcell_t	source_file_position;
	forth_cell_t	line_no;
	char file_buffer[FORTH_FILE_INPUT_BUFFER_LENGTH];
#endif
//	forth_cell_t	*wordlists;	// Wordlists in the search order.
//	forth_cell_t	wordlist_slots;	// The number of slots in the search order.
//	forth_cell_t	wordlist_cnt;	// The number of workdlists in the search order.
//	forth_cell_t	current;	// The current wordlist (where definitions are appended).
	forth_cell_t	defining;	// The word being defined.
	forth_cell_t	trace;		// Enabled execution trace.
	forth_cell_t	terminal_width;
	forth_cell_t	terminal_height;
	forth_cell_t	terminal_col;
	int (*page)(struct forth_runtime_context *rctx);							// PAGE -- If the device cannot do it set to to 0.
	int (*at_xy)(struct forth_runtime_context *rctx, forth_cell_t x, forth_cell_t y);			// AT-XY -- If the device cannot do it set to to 0.
	int (*write_string)(struct forth_runtime_context *rctx, const char *str, forth_cell_t length);		// TYPE
	int (*send_cr)(struct forth_runtime_context *rctx);							// CR
	forth_scell_t (*accept_string)(struct forth_runtime_context *rctx, char *buffer, forth_cell_t length);	// ACCEPT
	forth_cell_t (*key)(struct forth_runtime_context *rctx);						// KEY
	forth_cell_t (*key_q)(struct forth_runtime_context *rctx);						// KEY?
	forth_cell_t (*ekey)(struct forth_runtime_context *rctx);						// EKEY
	forth_cell_t (*ekey_q)(struct forth_runtime_context *rctx);						// EKEY?
	forth_cell_t (*ekey_to_char)(struct forth_runtime_context *rctx, forth_cell_t ekey);			// EKEY>CHAR
	forth_cell_t	tib_count;
	char		    tib[FORTH_TIB_SIZE];
	char 	       *numbuff_ptr;
	char		    num_buff[FORTH_NUM_BUFF_LENGTH];
	//char		    internal_buffer[32];
#if defined(FORTH_APPLICATION_DEFINED_CONTEXT_FIELDS)
	FORTH_APPLICATION_DEFINED_CONTEXT_FIELDS
#endif
#if defined(FORTH_USER_VARIABLES)
	forth_cell_t	user[FORTH_USER_VARIABLES];
#endif
};

extern const forth_vocabulary_entry_t forth_wl_forth[];
extern const forth_vocabulary_entry_t *forth_master_list_of_lists[];
extern const forth_vocabulary_entry_t forth_interpret_xt;

extern forth_scell_t forth_CATCH(forth_runtime_context_t *ctx, forth_xt_t xt);
extern forth_scell_t forth_RUN_INTERPRET(forth_runtime_context_t *ctx);
extern void forth_PRINT_ERROR(forth_runtime_context_t *ctx, forth_scell_t code);

extern const forth_vocabulary_entry_t *forth_SEARCH_LIST(const forth_vocabulary_entry_t *list, const char *name, int name_length);

extern void forth_THROW(forth_runtime_context_t *ctx, forth_scell_t code);
extern void forth_PUSH(forth_runtime_context_t *ctx, forth_ucell_t x);
extern forth_cell_t forth_POP(forth_runtime_context_t *ctx);
extern void forth_TYPE0(forth_runtime_context_t *ctx, const char *str);
extern void forth_EMIT(forth_runtime_context_t *ctx, char c);

// Primitives
extern void forth_execute(forth_runtime_context_t *ctx);
extern void forth_catch(forth_runtime_context_t *ctx);
extern void forth_throw(forth_runtime_context_t *ctx);
extern void forth_abort(forth_runtime_context_t *ctx);
extern void forth_print_error(forth_runtime_context_t *ctx);
extern void forth_type(forth_runtime_context_t *ctx);
extern void forth_emit(forth_runtime_context_t *ctx);
extern void forth_cr(forth_runtime_context_t *ctx);

extern void forth_dots(forth_runtime_context_t *ctx);
extern void forth_dot(forth_runtime_context_t *ctx);
extern void forth_hdot(forth_runtime_context_t *ctx);
extern void forth_udot(forth_runtime_context_t *ctx);

extern void forth_key(forth_runtime_context_t *ctx);
extern void forth_key_q(forth_runtime_context_t *ctx); // KEY?
extern void forth_ekey(forth_runtime_context_t *ctx);
extern void forth_ekey_q(forth_runtime_context_t *ctx); // EKEY?
extern void forth_ekey2char(forth_runtime_context_t *ctx); // EKEY>CHAR
extern void forth_accept(forth_runtime_context_t *ctx);
extern void forth_refill(forth_runtime_context_t *ctx);
extern void forth_space(forth_runtime_context_t *ctx);
extern void forth_spaces(forth_runtime_context_t *ctx);
extern void forth_dump(forth_runtime_context_t *ctx);

extern void forth_parse(forth_runtime_context_t *ctx);
// extern void forth_parse_word(forth_runtime_context_t *ctx);
extern void forth_parse_name(forth_runtime_context_t *ctx);
extern void forth_paren(forth_runtime_context_t *ctx);
extern void forth_dot_paren(forth_runtime_context_t *ctx);

extern void forth_depth(forth_runtime_context_t *ctx);
extern void forth_dup(forth_runtime_context_t *ctx);
extern void forth_drop(forth_runtime_context_t *ctx);
extern void forth_swap(forth_runtime_context_t *ctx);
extern void forth_over(forth_runtime_context_t *ctx);
extern void forth_2drop(forth_runtime_context_t *ctx);
extern void forth_2swap(forth_runtime_context_t *ctx);
extern void forth_2dup(forth_runtime_context_t *ctx);
extern void forth_2over(forth_runtime_context_t *ctx);

extern void forth_add(forth_runtime_context_t *ctx);
extern void forth_subtract(forth_runtime_context_t *ctx);
extern void forth_multiply(forth_runtime_context_t *ctx);
extern void forth_divide(forth_runtime_context_t *ctx);
extern void forth_mod(forth_runtime_context_t *ctx);
extern void forth_and(forth_runtime_context_t *ctx);
extern void forth_or(forth_runtime_context_t *ctx);
extern void forth_xor(forth_runtime_context_t *ctx);

extern void forth_find_name(struct forth_runtime_context *ctx);
extern void forth_tick(forth_runtime_context_t *ctx);

extern void forth_words(forth_runtime_context_t *ctx);
extern void forth_help(forth_runtime_context_t *ctx);
extern void forth_quit(forth_runtime_context_t *ctx);
extern void forth_bye(forth_runtime_context_t *ctx);

#endif