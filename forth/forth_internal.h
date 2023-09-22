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

#define FORTH_XT_FLAGS_IMMEDIATE 		0x80
#define FORTH_XT_FLAGS_ACTION_MASK		0x07
#define FORTH_XT_FLAGS_ACTION_PRIMITIVE	0x00
#define FORTH_XT_FLAGS_ACTION_CONSTANT	0x01
#define FORTH_XT_FLAGS_ACTION_VARIABLE	0x02
#define FORTH_XT_FLAGS_ACTION_DEFER		0x03
#define	FORTH_XT_FLAGS_ACTION_THREADED	0x04

struct forth_dictionary
{
	forth_ucell_t	dp;			// An index to items.
	forth_ucell_t	dp_max;		// Max value of dp.
	forth_ucell_t	latest;		// The location of the last defined word header, index to items.
	uint8_t 		items[1];	// Place holder for the rest of the dictionary.
};

typedef struct forth_dictionary forth_dictionary_t;

struct forth_runtime_context
{
	forth_dictionary_t *dictionary;
	forth_cell_t	*sp_max;
	forth_cell_t	*sp_min;
	forth_cell_t	*sp0;
	forth_cell_t	*sp;
	forth_cell_t	*rp_max;
	forth_cell_t	*rp_min;
	forth_cell_t	*rp0;
	forth_cell_t	*rp;
	// forth_xt_t		*ip;
	forth_cell_t		*ip;
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

#define FORTH_COLON_SYS_MARKER	0x4e4c4f43
#define FORTH_DEST_MARKER 		0x54534544
#define FORTH_ORIG_MARKER		0x4749524F

extern const forth_vocabulary_entry_t forth_wl_forth[];
extern const forth_vocabulary_entry_t forth_wl_system[];
extern const forth_vocabulary_entry_t *forth_master_list_of_lists[];
extern const forth_xt_t forth_interpret_xt;
extern const forth_xt_t forth_LIT_xt;
extern const forth_xt_t forth_XLIT_xt;
extern const forth_xt_t forth_SLIT_xt;
extern const forth_xt_t forth_BRANCH_xt;
extern const forth_xt_t forth_0BRANCH_xt;

extern forth_dictionary_t *forth_INIT_DICTIONARY(void *addr, forth_cell_t length);

extern void forth_PRINT_TRACE(forth_runtime_context_t *ctx, forth_xt_t xt);
extern void forth_EXECUTE(forth_runtime_context_t *ctx, forth_xt_t xt);
extern forth_scell_t forth_CATCH(forth_runtime_context_t *ctx, forth_xt_t xt);
extern forth_scell_t forth_RUN_INTERPRET(forth_runtime_context_t *ctx);
extern void forth_PRINT_ERROR(forth_runtime_context_t *ctx, forth_scell_t code);
extern forth_vocabulary_entry_t *forth_CREATE_DICTIONARY_ENTRY(forth_runtime_context_t *ctx);

extern const forth_vocabulary_entry_t *forth_SEARCH_LIST(const forth_vocabulary_entry_t *list, const char *name, int name_length);

extern void forth_CHECK_STACK_AT_LEAST(forth_runtime_context_t *ctx, forth_cell_t n);
extern void forth_THROW(forth_runtime_context_t *ctx, forth_scell_t code);
extern void forth_PUSH(forth_runtime_context_t *ctx, forth_ucell_t x);
extern forth_cell_t forth_POP(forth_runtime_context_t *ctx);
extern forth_dcell_t forth_DTOS_READ(forth_runtime_context_t *ctx);
extern forth_dcell_t forth_DPOP(forth_runtime_context_t *ctx);
extern forth_dcell_t forth_DPUSH(forth_runtime_context_t *ctx, forth_dcell_t ud);

extern void forth_TYPE0(forth_runtime_context_t *ctx, const char *str);
extern void forth_EMIT(forth_runtime_context_t *ctx, char c);
extern void forth_COMMA(forth_runtime_context_t *ctx, forth_cell_t x);
#define forth_COMPILE_COMMA(CTX , XT) forth_COMMA((CTX), (forth_cell_t)(XT))

extern void forth_InnerInterpreter(forth_runtime_context_t *ctx, forth_xt_t xt);
extern void forth_DoConst(forth_runtime_context_t *ctx, forth_xt_t xt);
extern void forth_DoVar(forth_runtime_context_t *ctx, forth_xt_t xt);

// Primitives
extern void forth_execute(forth_runtime_context_t *ctx);
extern void forth_trace_on(forth_runtime_context_t *ctx);
extern void forth_trace_off(forth_runtime_context_t *ctx);
extern void forth_paren_trace(forth_runtime_context_t *ctx); 		// (TRACE)
extern void forth_catch(forth_runtime_context_t *ctx);
extern void forth_throw(forth_runtime_context_t *ctx);
extern void forth_abort(forth_runtime_context_t *ctx);
extern void forth_print_error(forth_runtime_context_t *ctx);
extern void forth_type(forth_runtime_context_t *ctx);
extern void forth_emit(forth_runtime_context_t *ctx);
extern void forth_cr(forth_runtime_context_t *ctx);
extern void forth_at_xy(forth_runtime_context_t *ctx);
extern void forth_page(forth_runtime_context_t *ctx);

extern void forth_dots(forth_runtime_context_t *ctx);
extern void forth_dot(forth_runtime_context_t *ctx);
extern void forth_hdot(forth_runtime_context_t *ctx);
extern void forth_udot(forth_runtime_context_t *ctx);
extern void forth_dotr(forth_runtime_context_t *ctx);				// .R
extern void forth_udotr(forth_runtime_context_t *ctx);				// U.R

extern void forth_less_hash(forth_runtime_context_t *ctx);			// <#
extern void forth_hash(forth_runtime_context_t *ctx);				// #
extern void forth_hash_s(forth_runtime_context_t *ctx);				// #S
extern void forth_hold(forth_runtime_context_t *ctx);				// HOLD
extern void forth_holds(forth_runtime_context_t *ctx);				// HOLDS
extern void forth_sign(forth_runtime_context_t *ctx);				// SIGN
extern void forth_hash_greater(forth_runtime_context_t *ctx);		// #>

extern void forth_key(forth_runtime_context_t *ctx);
extern void forth_key_q(forth_runtime_context_t *ctx); 				// KEY?
extern void forth_ekey(forth_runtime_context_t *ctx);
extern void forth_ekey_q(forth_runtime_context_t *ctx); 			// EKEY?
extern void forth_ekey2char(forth_runtime_context_t *ctx); 			// EKEY>CHAR
extern void forth_accept(forth_runtime_context_t *ctx);
extern void forth_refill(forth_runtime_context_t *ctx);
extern void forth_space(forth_runtime_context_t *ctx);
extern void forth_spaces(forth_runtime_context_t *ctx);
extern void forth_dump(forth_runtime_context_t *ctx);

extern void forth_parse(forth_runtime_context_t *ctx);
extern void forth_parse_name(forth_runtime_context_t *ctx);
extern void forth_quot(forth_runtime_context_t *ctx);
extern void forth_squot(forth_runtime_context_t *ctx);
extern void forth_paren(forth_runtime_context_t *ctx);
extern void forth_dot_paren(forth_runtime_context_t *ctx);

extern void forth_invert(forth_runtime_context_t *ctx);
extern void forth_negate(forth_runtime_context_t *ctx);
extern void forth_abs(forth_runtime_context_t *ctx);
extern void forth_2mul(forth_runtime_context_t *ctx); 				// 2*
extern void forth_2div(forth_runtime_context_t *ctx); 				// 2*
extern void forth_sp_fetch(forth_runtime_context_t *ctx);			// SP@
extern void forth_sp0(forth_runtime_context_t *ctx);				// SP0
extern void forth_rp_fetch(forth_runtime_context_t *ctx);			// RP@
extern void forth_rp0(forth_runtime_context_t *ctx);				// RP0
extern void forth_depth(forth_runtime_context_t *ctx);
extern void forth_dup(forth_runtime_context_t *ctx);
extern void forth_question_dup(forth_runtime_context_t *ctx);
extern void forth_drop(forth_runtime_context_t *ctx);
extern void forth_nip(forth_runtime_context_t *ctx);
extern void forth_tuck(forth_runtime_context_t *ctx);
extern void forth_rot(forth_runtime_context_t *ctx);
extern void forth_mrot(forth_runtime_context_t *ctx); 				// -ROT
extern void forth_roll(forth_runtime_context_t *ctx);
extern void forth_pick(forth_runtime_context_t *ctx);
extern void forth_cspick(forth_runtime_context_t *ctx);
extern void forth_csroll(forth_runtime_context_t *ctx);
extern void forth_swap(forth_runtime_context_t *ctx);
extern void forth_over(forth_runtime_context_t *ctx);
extern void forth_fetch(forth_runtime_context_t *ctx); 				// @
extern void forth_store(forth_runtime_context_t *ctx); 				// !
extern void forth_plus_store(forth_runtime_context_t *ctx); 		// +!
extern void forth_1plus(forth_runtime_context_t *ctx);  			// 1+
extern void forth_1minus(forth_runtime_context_t *ctx); 			// 1-
extern void forth_cell_plus(forth_runtime_context_t *ctx); 			// CELL+
extern void forth_cells(forth_runtime_context_t *ctx);     			// CELLS
extern void forth_cfetch(forth_runtime_context_t *ctx); 			// C@
extern void forth_cstore(forth_runtime_context_t *ctx); 			// C!
extern void forth_questionmark(forth_runtime_context_t *ctx); 		// ?
extern void forth_2drop(forth_runtime_context_t *ctx);
extern void forth_2swap(forth_runtime_context_t *ctx);
extern void forth_2dup(forth_runtime_context_t *ctx);
extern void forth_2over(forth_runtime_context_t *ctx);
extern void forth_2rot(forth_runtime_context_t *ctx);
extern void forth_2fetch(forth_runtime_context_t *ctx); 			// 2@
extern void forth_2store(forth_runtime_context_t *ctx); 			// 2!

#if !defined(FORTH_NO_DOUBLES)
extern void forth_dzero_less(forth_runtime_context_t *ctx);			// D0<
extern void forth_dzero_equals(forth_runtime_context_t *ctx);		// D0=
extern void forth_dless(forth_runtime_context_t *ctx);				// D<
extern void forth_duless(forth_runtime_context_t *ctx);				// DU<
extern void forth_dequals(forth_runtime_context_t *ctx);			// D=
extern void forth_dnegate(forth_runtime_context_t *ctx);
extern void forth_dabs(forth_runtime_context_t *ctx);
extern void forth_dplus(forth_runtime_context_t *ctx);				// D+
extern void forth_dminus(forth_runtime_context_t *ctx);				// D-
extern void forth_s_to_d(forth_runtime_context_t *ctx);				// S>D
extern void forth_mplus(forth_runtime_context_t *ctx);				// M+
extern void forth_dmin(forth_runtime_context_t *ctx);				// DMIN
extern void forth_dmax(forth_runtime_context_t *ctx);				// DMAX
extern void forth_d2mul(forth_runtime_context_t *ctx);				// D2*
extern void forth_d2div(forth_runtime_context_t *ctx);				// D2/
extern void forth_ddot(forth_runtime_context_t *ctx);				// D.
#endif

extern void forth_to_r(forth_runtime_context_t *ctx);				// >R
extern void forth_r_fetch(forth_runtime_context_t *ctx);			// R@
extern void forth_r_from(forth_runtime_context_t *ctx);				// R>
extern void forth_2to_r(forth_runtime_context_t *ctx);				// 2>R
extern void forth_2r_fetch(forth_runtime_context_t *ctx);			// 2R@
extern void forth_2r_from(forth_runtime_context_t *ctx);			// 2R>
extern void forth_n_to_r(forth_runtime_context_t *ctx);				// N>R
extern void forth_n_r_from(forth_runtime_context_t *ctx);			// NR>

extern void forth_add(forth_runtime_context_t *ctx);
extern void forth_subtract(forth_runtime_context_t *ctx);
extern void forth_multiply(forth_runtime_context_t *ctx);
extern void forth_divide(forth_runtime_context_t *ctx);
extern void forth_mod(forth_runtime_context_t *ctx);				// MOD
extern void forth_div_mod(forth_runtime_context_t *ctx);			// /MOD
extern void forth_mult_div_mod(forth_runtime_context_t *ctx);		// */MOD
extern void forth_mult_div(forth_runtime_context_t *ctx);			// */
extern void forth_min(forth_runtime_context_t *ctx);
extern void forth_max(forth_runtime_context_t *ctx);
extern void forth_and(forth_runtime_context_t *ctx);
extern void forth_or(forth_runtime_context_t *ctx);
extern void forth_xor(forth_runtime_context_t *ctx);
extern void forth_equals(forth_runtime_context_t *ctx);				// =
extern void forth_not_equals(forth_runtime_context_t *ctx);			// <>
extern void forth_zero_equals(forth_runtime_context_t *ctx);		// 0=
extern void forth_zero_not_equals(forth_runtime_context_t *ctx);	// 0<>
extern void forth_uless(forth_runtime_context_t *ctx);				// U<
extern void forth_ugreater(forth_runtime_context_t *ctx);			// U>
extern void forth_less(forth_runtime_context_t *ctx);				// <
extern void forth_greater(forth_runtime_context_t *ctx);			// >
extern void forth_zero_less(forth_runtime_context_t *ctx);			// 0<
extern void forth_zero_greater(forth_runtime_context_t *ctx);		// 0>
extern void forth_fill(forth_runtime_context_t *ctx);
extern void forth_erase(forth_runtime_context_t *ctx);
extern void forth_blank(forth_runtime_context_t *ctx);
extern void forth_move(forth_runtime_context_t *ctx);
extern void forth_compare(forth_runtime_context_t *ctx);

extern void forth_find_name(struct forth_runtime_context *ctx);
extern void forth_bracket_defined(forth_runtime_context_t *ctx);
extern void forth_bracket_undefined(forth_runtime_context_t *ctx);
extern void forth_tick(forth_runtime_context_t *ctx);				// '
extern void forth_bracket_tick(forth_runtime_context_t *ctx);		// [']
extern void forth_char(forth_runtime_context_t *ctx);				// CHAR
extern void forth_bracket_char(forth_runtime_context_t *ctx);		// [CHAR]

extern void forth_evaluate(forth_runtime_context_t *ctx);
extern void forth_words(forth_runtime_context_t *ctx);
extern void forth_see(forth_runtime_context_t *ctx);
extern void forth_help(forth_runtime_context_t *ctx);
extern void forth_quit(forth_runtime_context_t *ctx);

extern void forth_noop(forth_runtime_context_t *ctx);
extern void forth_decimal(forth_runtime_context_t *ctx);
extern void forth_hex(forth_runtime_context_t *ctx);
extern void forth_base(forth_runtime_context_t *ctx);
extern void forth_state(forth_runtime_context_t *ctx);
extern void forth_to_in(forth_runtime_context_t *ctx); 				// >IN
extern void forth_source(forth_runtime_context_t *ctx);				// SOURCE
extern void forth_source_id(forth_runtime_context_t *ctx);			// SOURCE-ID
extern void forth_left_bracket(forth_runtime_context_t *ctx);
extern void forth_right_bracket(forth_runtime_context_t *ctx);

extern void forth_here(forth_runtime_context_t *ctx);
extern void forth_unused(forth_runtime_context_t *ctx);
extern void forth_align(forth_runtime_context_t *ctx);
extern void forth_aligned(forth_runtime_context_t *ctx);
extern void forth_allot(forth_runtime_context_t *ctx);
extern void forth_count(forth_runtime_context_t *ctx);
extern void forth_c_comma(forth_runtime_context_t *ctx); 			// C,
extern void forth_comma(forth_runtime_context_t *ctx);	 			// ,

extern void forth_ahead(forth_runtime_context_t *ctx);
extern void forth_if(forth_runtime_context_t *ctx);					// IF
extern void forth_else(forth_runtime_context_t *ctx);				// ELSE
extern void forth_then(forth_runtime_context_t *ctx);				// THEN
extern void forth_begin(forth_runtime_context_t *ctx);
extern void forth_again(forth_runtime_context_t *ctx);
extern void forth_until(forth_runtime_context_t *ctx);
extern void forth_while(forth_runtime_context_t *ctx);
extern void forth_repeat(forth_runtime_context_t *ctx);

extern void forth_literal(forth_runtime_context_t *ctx);
extern void forth_xliteral(forth_runtime_context_t *ctx);
extern void forth_2literal(forth_runtime_context_t *ctx);
extern void forth_sliteral(forth_runtime_context_t *ctx);
extern void forth_variable(forth_runtime_context_t *ctx);
extern void forth_constant(forth_runtime_context_t *ctx);
extern void forth_colon_noname(forth_runtime_context_t *ctx);
extern void forth_colon(forth_runtime_context_t *ctx);
extern void forth_semicolon(forth_runtime_context_t *ctx);
extern void forth_immediate(forth_runtime_context_t *ctx);
extern void forth_recurse(forth_runtime_context_t *ctx);

extern void forth_bye(forth_runtime_context_t *ctx);

#endif