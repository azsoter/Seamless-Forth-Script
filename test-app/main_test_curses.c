/*
 * Test program to run on Linux/POSIX.
 * for the Embeddable Forth Command Interpreter.
 * Written by Andras Zsoter.
 * This is a quick and dirty example how to control the Forth Engine on a terminal.
 * You can treat the contents of this file as public domain.
 *
 * Attribution is appreciated but not mandatory for the contents of this file.
 *
 */

// http://forth.teleonomix.com/
#include <forth.h>
#include <forth_internal.h>

#include <stdio.h>
#include <string.h>
#include <curses.h>

#define DICTIONARY_SIZE 512 /* cells */
#define STACK_SIZE 256 /* cells */
#define SEARCH_ORDER_SIZE 32

forth_cell_t dictionary[DICTIONARY_SIZE];
forth_cell_t data_stack[STACK_SIZE];
forth_cell_t return_stack[STACK_SIZE];
forth_runtime_context_t r_ctx;
//struct forth_persistent_context p_ctx;

forth_cell_t search_order[SEARCH_ORDER_SIZE];

int reset_curses(void)
{
	cbreak();
	noecho();
 	immedok(stdscr,TRUE);
	scrollok(stdscr,TRUE);
	idlok(stdscr,TRUE);
 	keypad(stdscr,TRUE);
 	nonl();
	return 0; // No trivial to check.
}

int init_curses(void)
{
	WINDOW *w;
	w = initscr();
	start_color();
	reset_curses();
	return (0 == w) ? -1 : 0;
}

int close_curses(void)
{
	return (ERR == endwin()) ? -1 : 0;
}

int emit_char(char c)
{
	if ('\r' == c)
	{
		c = '\n';
	}

	return (ERR == addch(c)) ? -1 : 0;
}

int write_str(struct forth_runtime_context *rctx, const char *str, forth_cell_t length)
{
	rctx->terminal_col += length;

	if (0 == str)
	{
		return -1;
	}

	if (0 != length)
	{
		addnstr(str, length);
	}

	return 0;
}

int page(struct forth_runtime_context *rctx)
{
	clear();
	rctx->terminal_col = 0;
	return 0;
}

int send_cr(struct forth_runtime_context *rctx)
{
	emit_char('\n');
	rctx->terminal_col = 0;
	return 0;
}

int at_xy(struct forth_runtime_context *rctx, forth_cell_t x, forth_cell_t y)
{
	move(y, x);
	return 0;
}

// These are fake, in reality they should be more complicated to follow the rules in the FORTH standard.
// KEY? and EKEY? using curses.
forth_cell_t key(struct forth_runtime_context *rctx)
{
	char c = getch();

	if (127 == c || 8 == c || 7 == c) 	// Ugly, but I have no idea what the proper symbolic names are.
	{
		c = '\b';
	}

	return (forth_cell_t)c;
}

forth_cell_t key_q(struct forth_runtime_context *rctx)
{
	int c = getch();

	if (ERR == c)
	{
		return FORTH_FALSE;
	}
	ungetch(c);
	return 1;
}

forth_cell_t ekey(struct forth_runtime_context *rctx)
{
	char c = getch();
	return ((forth_cell_t)c) << 8;
}

forth_cell_t ekey_q(struct forth_runtime_context *rctx)
{
	int c = getch();

	if (ERR == c)
	{
		return FORTH_FALSE;
	}
	ungetch(c);
	return 1;
}

forth_cell_t ekey_to_char(struct forth_runtime_context *rctx, forth_cell_t ek)
{
	char c =  ek >> 8;

	if (127 == c || 8 == c || 7 == c) 	// Ugly, but I have no idea what the proper symbolic names are.
	{
		c = '\b';
	}

	return c;
}

forth_scell_t accept_str(struct forth_runtime_context *rctx, char *buffer, forth_cell_t length)
{
	// char *res;
	forth_cell_t count = 0;
	char k;

	while (count < length)
	{
		k = key(rctx);

		switch(k)
		{
			case '\r':
			case '\n':
				emit_char(FORTH_CHAR_SPACE);
			return count;

			case '\b':
				if (count > 0)
				{
					emit_char(8);
					delch();
					count--;
				}
				
			break;

			default:
				buffer[count++] = k;
				emit_char(k);
			break;
		}

	}

	return count;
}

int main()
{
	forth_scell_t res;
	forth_dictionary_t *dict;
	forth_context_init_data_t init_data = { 0 };

   	memset(&r_ctx, 0, sizeof(r_ctx));
   	memset(search_order, 0, sizeof(search_order));
   	memset(data_stack,0, sizeof(data_stack));
   	memset(return_stack, 0, sizeof(return_stack));

	init_data.data_stack = data_stack;
	init_data.data_stack_cell_count = STACK_SIZE;
	init_data.return_stack = return_stack;
	init_data.return_stack_cell_count = STACK_SIZE;

#if !defined(FORTH_WITHOUT_COMPILATION)
	if (0 == dict)
	{
		dict = Forth_InitDictionary(dictionary, sizeof(dictionary));
	}

	init_data.dictionary = dict;
	init_data.search_order = search_order;
	init_data.search_order_slots = (SEARCH_ORDER_SIZE);

	res = Forth_InitContext(&r_ctx, &init_data);

	if (0 > res)
	{
		return (int)res;
	}

#endif

	r_ctx.terminal_width = 80;
	r_ctx.terminal_height = 25;
	r_ctx.write_string = &write_str;
	r_ctx.page = &page;
	r_ctx.send_cr = &send_cr;
	r_ctx.accept_string = &accept_str;
	r_ctx.key = &key;
	r_ctx.key_q = &key_q;
	r_ctx.ekey = &ekey;
	r_ctx.ekey_q = &ekey_q;
	r_ctx.ekey_to_char = &ekey_to_char;
	r_ctx.at_xy = &at_xy;

	init_curses();
	
	(void)Forth(&r_ctx, "quit", 4, 1);

	close_curses();

	return 0;
}


