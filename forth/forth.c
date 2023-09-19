/*
* forth.c
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

#include <string.h>
#include <ctype.h>
#include <forth.h>
#include <forth_internal.h>

#include <stdio.h>
// ---------------------------------------------------------------------------------------------------------------
//                                                  STACK Gymnastics, CATCH and THROW
// ---------------------------------------------------------------------------------------------------------------
void forth_THROW(forth_runtime_context_t *ctx, forth_scell_t code)
{
    jmp_buf *handler = (jmp_buf *)(ctx->throw_handler);

    if (0 != code)
    {
        if (0 == handler)
        {
            forth_quit(ctx);
        }
    
        // **TODO** still need to handle ABORT" (code = -2).
        longjmp(*handler, (int)code);
    }
}

// Push an item to the data stack, perform stack checking.
void forth_PUSH(forth_runtime_context_t *ctx, forth_ucell_t x)
{
    ctx->sp -= 1;
    if (ctx->sp < ctx->sp_min)
    {
        forth_THROW(ctx, -3);
    }
    *(ctx->sp) = x;
}

// Pop an item from the data stack, perform stack checking.
forth_cell_t forth_POP(forth_runtime_context_t *ctx)
{
    forth_cell_t x = *(ctx->sp++);

    if (ctx->sp > ctx->sp_max)
    {
        forth_THROW(ctx, -4);
    }
    return x;
}

void forth_EXECUTE(forth_runtime_context_t *ctx, forth_xt_t xt)
{
    if (0 == xt)
    {
        forth_THROW(ctx, -13); // Is there a better value to throw here???????
    }

    switch((uint8_t)(xt->flags & FORTH_XT_FLAGS_ACTION_MASK))
	{
		case FORTH_XT_FLAGS_ACTION_PRIMITIVE:
		    ((forth_behavior_t)xt->meaning)(ctx);
			break;

		case FORTH_XT_FLAGS_ACTION_THREADED:
			forth_InnerInterpreter(ctx, xt);
			break;

		case FORTH_XT_FLAGS_ACTION_VARIABLE:
			forth_DoVar(ctx, xt);
			break;

		case FORTH_XT_FLAGS_ACTION_CONSTANT:
			forth_DoConst(ctx, xt);
			break;

		default:
			forth_THROW(ctx, -21);
			break;
	}

}

// EXECUTE ( xt -- )
void forth_execute(forth_runtime_context_t *ctx)
{
    forth_xt_t xt = (forth_xt_t)forth_POP(ctx);
	forth_EXECUTE(ctx, xt);
}

// ABORT ( -- )
void forth_abort(forth_runtime_context_t *ctx)
{
    forth_THROW(ctx, -1);
}

// THROW ( code|0 -- )
void forth_throw(forth_runtime_context_t *ctx)
{
    forth_scell_t code = (forth_scell_t)forth_POP(ctx);
    forth_THROW(ctx, code);
}

// CATCH ( xt -- code|0 )
void forth_catch(forth_runtime_context_t *ctx)
{
    int res;
    forth_ucell_t *saved_sp = ctx->sp;
    forth_ucell_t *saved_rp = ctx->rp;
    forth_ucell_t saved_handler = ctx->throw_handler;
	forth_xt_t *saved_ip = ctx->ip;
    jmp_buf catch_frame;

    res = setjmp(catch_frame);

    if (0 == res)
    {
        ctx->throw_handler = (forth_ucell_t)(&catch_frame);
        forth_execute(ctx);
        forth_PUSH(ctx, 0);
    }
    else
    {
		ctx->ip = saved_ip;
        ctx->sp = saved_sp;
        ctx->rp = saved_rp;
        *(ctx->sp) = res;
    }

    ctx->throw_handler = saved_handler;
}

// This function is an interface to make it easier to call catch from C.
// It takes the execution token as a parameter and returns the code from catch.
forth_scell_t forth_CATCH(forth_runtime_context_t *ctx, forth_xt_t xt)
{
    forth_scell_t res;

    if ((ctx->sp - 1) < ctx->sp_min)
    {
        return -3;
    }

    if (ctx->sp > ctx->sp_max)
    {
        return -4;
    }

    *--(ctx->sp) = (forth_cell_t)xt;

    forth_catch(ctx);

    res = *(ctx->sp++);

    return res;
}

// DEPTH ( -- depth )
void forth_depth(forth_runtime_context_t *ctx)
{
    forth_PUSH(ctx, (forth_cell_t)(ctx->sp0 - ctx->sp));
}

// DUP ( x -- x x )
void forth_dup(forth_runtime_context_t *ctx)
{
    forth_PUSH(ctx, *(ctx->sp));
}

// DROP ( x -- )
void forth_drop(forth_runtime_context_t *ctx)
{
    (void)forth_POP(ctx);
}

// SWAP ( x y -- y x )
void forth_swap(forth_runtime_context_t *ctx)
{
    forth_cell_t x;

    if ((ctx->sp + 1) > ctx->sp_max)
    {
        forth_THROW(ctx, -4);
    }

    x = ctx->sp[0];
    ctx->sp[0] = ctx->sp[1];
    ctx->sp[1] = x;
}

// OVER ( x y -- x y x )
void forth_over(forth_runtime_context_t *ctx)
{
    if ((ctx->sp + 1) > ctx->sp_max)
    {
        forth_THROW(ctx, -4);
    }

    forth_PUSH(ctx, ctx->sp[1]);
}

// @ ( addr - val )
void forth_fetch(forth_runtime_context_t *ctx)
{
	forth_cell_t *p = (forth_cell_t *)forth_POP(ctx);
	forth_cell_t  c = *p;
	forth_PUSH(ctx, c);
}

// ! ( val addr - )
void forth_store(forth_runtime_context_t *ctx)
{
	forth_cell_t *p = (forth_cell_t *)forth_POP(ctx);
	forth_cell_t  c = forth_POP(ctx);
	*p = c;
}

// ------------------------------------------------
// 2DROP ( x y -- )
void forth_2drop(forth_runtime_context_t *ctx)
{
    if ((ctx->sp + 1) > ctx->sp_max)
    {
        forth_THROW(ctx, -4);
    }
    ctx->sp += 2;
}

// 2SWAP ( x y a b -- a b y x )
void forth_2swap(forth_runtime_context_t *ctx)
{
    forth_cell_t x;

    if ((ctx->sp + 3) > ctx->sp_max)
    {
        forth_THROW(ctx, -4);
    }

    x = ctx->sp[0];
    ctx->sp[0] = ctx->sp[2];
    ctx->sp[2] = x;

    x = ctx->sp[1];
    ctx->sp[1] = ctx->sp[3];
    ctx->sp[3] = x;
}

// 2DUP ( x y -- x y y x )
void forth_2dup(forth_runtime_context_t *ctx)
{
    forth_over(ctx);
    forth_over(ctx);
}

// 2OVER ( x y a b -- x y a b y x )
void forth_2over(forth_runtime_context_t *ctx)
{
    forth_cell_t x;
    forth_cell_t y;

    if ((ctx->sp + 3) > ctx->sp_max)
    {
        forth_THROW(ctx, -4);
    }

    x = ctx->sp[3];
    y = ctx->sp[2];
    
    forth_PUSH(ctx, x);
    forth_PUSH(ctx, y);
}
// ---------------------------------------------------------------------------------------------------------------
//                                                  Arithmetics
// ---------------------------------------------------------------------------------------------------------------
// + ( x y -- x+y )
void forth_add(forth_runtime_context_t *ctx)
{
    forth_cell_t y = forth_POP(ctx);
    forth_cell_t x = forth_POP(ctx);
    forth_PUSH(ctx, x+y);
}

// - ( x y -- x-y )
void forth_subtract(forth_runtime_context_t *ctx)
{
    forth_cell_t y = forth_POP(ctx);
    forth_cell_t x = forth_POP(ctx);
    forth_PUSH(ctx, x-y);
}

// * ( x y -- x*y )
void forth_multiply(forth_runtime_context_t *ctx)
{
    forth_cell_t y = forth_POP(ctx);
    forth_cell_t x = forth_POP(ctx);
    forth_PUSH(ctx, x*y);
}

// / ( x y -- x/y )
void forth_divide(forth_runtime_context_t *ctx)
{
    forth_scell_t y = (forth_scell_t)forth_POP(ctx);
    forth_scell_t x = (forth_scell_t)forth_POP(ctx);
    if (0 == y)
    {
        forth_THROW(ctx, -10);
    }
    forth_PUSH(ctx, (forth_cell_t)(x/y));
}

// MOD ( x y -- x%y )
void forth_mod(forth_runtime_context_t *ctx)
{
    forth_scell_t y = (forth_scell_t)forth_POP(ctx);
    forth_scell_t x = (forth_scell_t)forth_POP(ctx);
    if (0 == y)
    {
        forth_THROW(ctx, -10);
    }
    forth_PUSH(ctx, (forth_cell_t)(x%y));
}

// AND ( x y -- x&y )
void forth_and(forth_runtime_context_t *ctx)
{
    forth_cell_t y = forth_POP(ctx);
    forth_cell_t x = forth_POP(ctx);
    forth_PUSH(ctx, x&y);
}

// OR ( x y -- x|y )
void forth_or(forth_runtime_context_t *ctx)
{
    forth_cell_t y = forth_POP(ctx);
    forth_cell_t x = forth_POP(ctx);
    forth_PUSH(ctx, x|y);
}

// XOR ( x y -- x^y )
void forth_xor(forth_runtime_context_t *ctx)
{
    forth_cell_t y = forth_POP(ctx);
    forth_cell_t x = forth_POP(ctx);
    forth_PUSH(ctx, x^y);
}

// ---------------------------------------------------------------------------------------------------------------
//                                                  Terminal I/O
// ---------------------------------------------------------------------------------------------------------------
// TYPE ( c-addr len -- )
void forth_type(forth_runtime_context_t *ctx)
{
    int res;
    forth_cell_t len = forth_POP(ctx);
    forth_cell_t addr = forth_POP(ctx);

    if ((0 == addr) || (0 == len))
    {
        return;
    }

    res = ctx->write_string(ctx, (const char *)addr, len);

    if (0 > res)
    {
        forth_THROW(ctx, -57);
    }
}

// TYPE0 ( asciiz-string-address -- )
void forth_TYPE0(forth_runtime_context_t *ctx, const char *str)
{
    int res;
    size_t len;

	if (0 == str)
	{
		return;
	}

	len = strlen(str);

	if (0 == len)
	{
		return;
	}

	res = ctx->write_string(ctx, str, len);
    // **TODO** Should we do a throw if failed????
}

void forth_EMIT(forth_runtime_context_t *ctx, char c)
{
    int res = ctx->write_string(ctx, &c, 1);

    if (0 > res)
    {
        forth_THROW(ctx, -57);
    }
}

// EMIT ( c -- )
void forth_emit(forth_runtime_context_t *ctx)
{
    forth_cell_t chr = forth_POP(ctx);
    forth_EMIT(ctx, (char)chr);
}

// CR ( -- )
void forth_cr(forth_runtime_context_t *ctx)
{
    int res = ctx->send_cr(ctx);

    if (0 > res)
    {
        forth_THROW(ctx, -57);
    }
}
// -------------------------------------
// EKEY>CHAR ( key-event -- key-event false | char true )
void forth_ekey2char(forth_runtime_context_t *ctx)
{
    forth_cell_t res;
    forth_cell_t event;

	if (0 == ctx->ekey_to_char)
	{
		forth_THROW(ctx, -21);
	}

    event = forth_POP(ctx);

	res = ctx->ekey_to_char(ctx, event);

	if (FORTH_TRUE == res)
	{
        forth_PUSH(ctx, event);
		forth_PUSH(ctx, FORTH_FALSE);
	}
    else
    {
        forth_PUSH(ctx, res);
        forth_PUSH(ctx, FORTH_TRUE);
    }
}

// EKEY? ( -- flag )
void forth_ekey_q(forth_runtime_context_t *ctx)
{
    forth_cell_t res;

	if (0 == ctx->ekey_q)
	{
		forth_THROW(ctx, -21);
	}

	res = ctx->ekey_q(ctx);

	if (0 > res)
	{
		forth_THROW(ctx, -57);
	}

	forth_PUSH(ctx, res ? FORTH_TRUE : FORTH_FALSE);
}

// EKEY ( -- key-event )
void forth_ekey(forth_runtime_context_t *ctx)
{
    forth_cell_t res;

	if (0 == ctx->ekey)
	{
		forth_THROW(ctx, -21);
	}

	res = ctx->ekey(ctx);

	if (FORTH_TRUE == res)
	{
		forth_THROW(ctx, -57);
	}

	forth_PUSH(ctx, res);
}

// KEY? ( -- flag )
void forth_key_q(forth_runtime_context_t *ctx)
{
    forth_cell_t res;

	if (0 == ctx->key_q)
	{
		forth_THROW(ctx, -21);
	}

	res = ctx->key_q(ctx);

	if (0 > res)
	{
		forth_THROW(ctx, -57);
	}

	forth_PUSH(ctx, res ? FORTH_TRUE : FORTH_FALSE);
}

// KEY ( -- key )
void forth_key(forth_runtime_context_t *ctx)
{
    forth_cell_t res;

	if (0 == ctx->key)
	{
		forth_THROW(ctx, -21);
	}

	res = ctx->key(ctx);

	if (FORTH_TRUE == res)
	{
		forth_THROW(ctx, -57);
	}

	forth_PUSH(ctx, res);
}

// ACCEPT ( buff-addr buff-len -- str-len )
void forth_accept(forth_runtime_context_t *ctx)
{
    forth_cell_t len = forth_POP(ctx);
    forth_cell_t buffer_addr = forth_POP(ctx);
	forth_scell_t l;

    if (0 == ctx->accept_string)
	{
		forth_THROW(ctx, -21);
	}

    l = ctx->accept_string(ctx, (char *)buffer_addr, len);
#if 0
    if (0 > l)
    {
        forth_THROW(ctx, -57);
    }
#endif
    forth_PUSH(ctx, l);
}

// SPACE (  -- )
void forth_space(forth_runtime_context_t *ctx)
{
    forth_EMIT(ctx, FORTH_CHAR_SPACE);
}

// SPACES ( n -- )
void forth_spaces(forth_runtime_context_t *ctx)
{
    forth_scell_t len = (forth_scell_t)forth_POP(ctx);
    while(0 < len)
    {
        forth_EMIT(ctx, FORTH_CHAR_SPACE);
        len--;
    }
}

// REFILL ( -- flag )
void forth_refill(forth_runtime_context_t *ctx)
{
    forth_scell_t res;
    if (0 == ctx->source_id)
    {
        ctx->blk = 0;
        ctx->source_length = 0;
        ctx->source_address = ctx->tib;
        ctx->to_in = 0;
        if (0 == ctx->accept_string)
        {
            forth_PUSH(ctx, FORTH_FALSE);
        }
        else
        {
            res = ctx->accept_string(ctx, ctx->tib, FORTH_TIB_SIZE);

            if (0 > res)
            {
                forth_PUSH(ctx, FORTH_FALSE);
            }
            else
            {
                ctx->tib_count = res;
                ctx->source_length = res;
                forth_PUSH(ctx, FORTH_TRUE);   
            }
        }

    }
    else if (-1 == ctx->source_id)
    {
        forth_PUSH(ctx, FORTH_FALSE);
    }
    else
    {
#if defined(FORTH_INCLUDE_FILE_ACCESS_WORDS)
        // We could have someting here if files were supported.......
#endif
        forth_PUSH(ctx, FORTH_FALSE);
    }
}
// ---------------------------------------------------------------------------------------------------------------
//                                                  Printing Numbers
// ---------------------------------------------------------------------------------------------------------------
static char forth_VAL2DIGIT(forth_byte_t val)
{
	return (char)((val < 10) ? ( val + '0') : ((val - 10) +'A'));
} 

static char *forth_FORMAT_UNSIGNED(forth_cell_t value, forth_cell_t base, forth_byte_t width, char *buff)
{
	char *p = buff;
	forth_byte_t len = 0;

 	if (base < 2)
	{
		base = 10;
	}

 	do {
		*(--p) = forth_VAL2DIGIT(value % base);
		value /= base;
		len += 1;
    	} while(value);

 	while (width > len)
	{
		*(--p) = '0';
		len++;
	}
 
	return p; 
}

static int forth_HDOT(struct forth_runtime_context *ctx, forth_cell_t value)
{
	char *buffer = ctx->num_buff;
	char *end = buffer + (FORTH_CELL_HEX_DIGITS) + 1;
	char *p;
	*end = FORTH_CHAR_SPACE;
	p = forth_FORMAT_UNSIGNED(value, 16, FORTH_CELL_HEX_DIGITS, end);
	return ctx->write_string(ctx, p, (end - p) + 1);
}

static int forth_UDOT(struct forth_runtime_context *ctx, forth_cell_t base, forth_cell_t value)
{
	char *buffer = ctx->num_buff;
	char *end = buffer + sizeof(ctx->num_buff)/* + 2*/;
	char *p;
	*end = FORTH_CHAR_SPACE;
	p = forth_FORMAT_UNSIGNED(value, base, 1, end);
	return ctx->write_string(ctx, p, (end - p) + 1);
}

static int forth_DOT(struct forth_runtime_context *ctx, forth_cell_t base, forth_cell_t value)
{
	char *buffer = ctx->num_buff;
	char *end = buffer + sizeof(ctx->num_buff)/* + 2*/;
	forth_scell_t val = (forth_scell_t)value;
	char *p;
	*end = FORTH_CHAR_SPACE;
	if (val < 0)
	{
		value = (forth_cell_t)-val;
	}
	p = forth_FORMAT_UNSIGNED(value, base, 1, end);
	if (val < 0)
	{
		*--p = '-';
	}
	return ctx->write_string(ctx, p, (end - p) + 1);
}

static int forth_DOT_R(struct forth_runtime_context *ctx, forth_cell_t base, forth_cell_t value, forth_cell_t width, forth_cell_t is_signed)
{
	char *buffer = ctx->num_buff;
	char *end = buffer + sizeof(ctx->num_buff)/* + 2*/;
	forth_scell_t val = (forth_scell_t)value;
	char *p;
	forth_cell_t nlen;
	forth_cell_t i;

	char c =  FORTH_CHAR_SPACE;
	if (is_signed && (val < 0))
	{
		value = (forth_cell_t)-val;
	}

	p = forth_FORMAT_UNSIGNED(value, base, 1, end);

	if (is_signed && (val < 0))
	{
		*--p = '-';
	}

	nlen = (end - p);

	// printf("nlen = %u\n", nlen);

	for (i = nlen; i < width; i++)
	{
		ctx->write_string(ctx, &c, 1);
	}

	return ctx->write_string(ctx, p, nlen);
}

int forth_DOTS(forth_runtime_context_t *ctx)
{
	char buff[20];
	char *p;
	char *end;
	forth_cell_t *sp;
	int res;
	forth_scell_t item;

	end = buff + sizeof(buff);
	p = end - 1;
	*p = FORTH_CHAR_SPACE;
	*--p = ']';

	p = forth_FORMAT_UNSIGNED(ctx->sp0 - ctx->sp, 10, 1, p);

	*--p = '[';

	res = ctx->write_string(ctx, p, (end - p));

	if (res < 0)
	{
		return res;
	}

	for (sp = ctx->sp0 - 1; sp >= ctx->sp; sp--)
	{
		item = *sp;

		if (item < 0)
		{
			item = -item;
		}

		p = forth_FORMAT_UNSIGNED((forth_cell_t)item, ctx->base, 1, end - 1);

		if (((forth_scell_t)*sp) < 0)
		{
			*--p = '-';
		}

		res = ctx->write_string(ctx, p, (end - p));

		if (res < 0)
		{
			return res;
		}
	}

	return ctx->send_cr(ctx);
}

// -----------------------------------------------------------------
// . ( x -- )
void forth_dot(forth_runtime_context_t *ctx)
{
    int res;
    forth_cell_t value;
    forth_cell_t base = ctx->base;
    
    if (0 == base)
    {
        base = 10;
    }

    value = forth_POP(ctx);

    res = forth_DOT(ctx, base, value);

    if (0 > res)
    {
        forth_THROW(ctx, -57);
    }
}

// U. ( x -- )
void forth_udot(forth_runtime_context_t *ctx)
{
    int res;
    forth_cell_t value;
    forth_cell_t base = ctx->base;
    
    if (0 == base)
    {
        base = 10;
    }

    value = forth_POP(ctx);

    res = forth_UDOT(ctx, base, value);

    if (0 > res)
    {
        forth_THROW(ctx, -57);
    }
}

// H. ( x -- )
void forth_hdot(forth_runtime_context_t *ctx)
{
    int res;
    forth_cell_t value = forth_POP(ctx);
    res = forth_HDOT(ctx, value);

    if (0 > res)
    {
        forth_THROW(ctx, -57);
    }
}

// .S ( -- )
void forth_dots(forth_runtime_context_t *ctx)
{
    int res = forth_DOTS(ctx);
    if (0 > res)
    {
        forth_THROW(ctx, -57);
    }
}

int forth_DUMP(struct forth_runtime_context *ctx, const char *addr, forth_cell_t len)
{
	char byte_buffer[3];
	char buff[8];
	forth_cell_t i;
	char c;
	forth_cell_t cnt;

	byte_buffer[2] = FORTH_CHAR_SPACE;

	if (0 == len)
	{
		return 0;
	}

	for(i = 0; i < len; i++)
	{
 		if ( 0 == (i % 8))
	 	{
			if (i)
            {
                if (0 > ctx->write_string(ctx, buff,8))
				{
					return -1;
				}
            }
			ctx->send_cr(ctx);
			forth_HDOT(ctx, (forth_cell_t)addr);
            forth_TYPE0(ctx, ": "); 
			memset(buff,FORTH_CHAR_SPACE, 8);
	 	}
		c = *addr++;
		// Check for printable characters.
 		buff[i % 8] = ( (c <128) && (c>31) ) ? c : '.';
		byte_buffer[0] = forth_VAL2DIGIT(0x0F & (c >> 4));
		byte_buffer[1] = forth_VAL2DIGIT(0x0F & c);
      	if (0 > ctx->write_string(ctx, byte_buffer, 3))
		{
			return -1;
		}
    }

	cnt = (i % 8) ? (i % 8) : 8;

	if (8 != cnt)
	{
		byte_buffer[0] = FORTH_CHAR_SPACE;
		byte_buffer[1] = FORTH_CHAR_SPACE;

		for (i = (8 - cnt); i != 0; i--)
		{
      		if (0 > ctx->write_string(ctx, byte_buffer, 3))
			{
				return -1;
			}
		}		
	}

   	ctx->write_string(ctx, buff, cnt);
	return ctx->send_cr(ctx);
}

// DUMP ( addr count -- )
void forth_dump(forth_runtime_context_t *ctx)
{
    int res;
    forth_cell_t count = forth_POP(ctx);
    forth_cell_t addr = forth_POP(ctx);

    res = forth_DUMP(ctx, (const char *)addr, count);

    if (0 > res)
    {
        forth_THROW(ctx, -57);
    }
}
// ---------------------------------------------------------------------------------------------------------------
//                                                  Input Parsing
// ---------------------------------------------------------------------------------------------------------------
static void forth_SKIP_DELIMITERS(const char **buffer, forth_cell_t *length, char delimiter)
{
	const char *buff = *buffer;
	forth_cell_t len = *length;

	if ((FORTH_CHAR_SPACE) == delimiter)
	{
		while ((0 != len) && isspace(*buff))
		{
			buff++;
			len--;
		}
	}
	else
	{
		while ((0 != len) && (delimiter == *buff))
		{
			buff++;
			len--;
		}
	}

	*buffer = buff;
	*length = *length - len;
}

static void forth_PARSE_TILL_DELIMITER(const char **buffer, forth_cell_t *length, char delimiter)
{
	const char *buff = *buffer;
	forth_cell_t len = *length;

#if defined(DEBUG_PARSE)
	printf("-----------------------------------------------------------------\n");
	write_str(buff, len);
	printf("\n-----------------------------------------------------------------\n");
#endif
	if ((FORTH_CHAR_SPACE) == delimiter)
	{
		while ((0 != len) && !isspace(*buff))
		{
			// putchar(*buff);
			buff++;
			len--;
		}
	}
	else
	{
		while ((0 != len) && (delimiter != *buff))
		{
			// putchar(*buff);
			buff++;
			len--;
		}
	}

	// *buffer = buff;
	*length = *length - len;
}

// ( delim -- c-addr len|0 )
void forth_parse(forth_runtime_context_t *ctx)
{
    char delimiter;
	const char *address;
	forth_cell_t length;

    delimiter = (char)forth_POP(ctx);

#if defined(DEBUG_PARSE)
	printf("%s: addr = %p, len = %u, >IN = %u\n", __FUNCTION__, rctx->source_address, rctx->source_length, rctx->to_in);
#endif
	if (ctx->to_in < ctx->source_length)
	{
		address = ctx->source_address + ctx->to_in;
		length  = ctx->source_length - ctx->to_in;

#if defined(DEBUG_PARSE)
		printf("%s (start) address=%p length=%u\n", __FUNCTION__, address, length);
#endif
		forth_PARSE_TILL_DELIMITER(&address, &length, delimiter);
#if defined(DEBUG_PARSE)
		printf("%s (ret) address=%p length=%u\n", __FUNCTION__, address, length);
		rctx->write_string(rctx, address, length);
#endif

		ctx->to_in += length;

		if (ctx->to_in < ctx->source_length)
		{
			ctx->to_in++;
		}

		//*--(rctx->sp) = (forth_cell_t)address;
		//*--(rctx->sp) = length;
        forth_PUSH(ctx, (forth_cell_t)address);
        forth_PUSH(ctx, length);
	}
	else
	{
		//*--(rctx->sp) = 0;
		//*--(rctx->sp) = 0;
        forth_PUSH(ctx, 0);
        forth_PUSH(ctx, 0);
	}
}

// https://forth-standard.org/standard/core/PARSE-NAME
// ( "name" -- c-addr len|0 )
void forth_parse_name(forth_runtime_context_t *ctx)
{
	const char *address;
	forth_cell_t length;
    char dlm = FORTH_CHAR_SPACE;
    //char dlm = (char)forth_POP(ctx);


#if defined(DEBUG_PARSE)
	printf("%s: addr = %p, len = %u, >IN = %u\n", __FUNCTION__, rctx->source_address, rctx->source_length, rctx->to_in);
	rctx->write_string(rctx, rctx->source_address, rctx->source_length); puts("");
#endif

	if (ctx->to_in < ctx->source_length)
	{
		address = ctx->source_address + ctx->to_in;
		length = ctx->source_length - ctx->to_in;

#if defined(DEBUG_PARSE)
		printf("%s: >>Skip: addr = %p, len = %u\n", __FUNCTION__, address, length);
#endif
		forth_SKIP_DELIMITERS(&address, &length, dlm);
#if defined(DEBUG_PARSE)
		printf("%s: <<Skip: addr = %p, len = %u\n", __FUNCTION__, address, length);
#endif

		ctx->to_in += length;

        forth_PUSH(ctx, dlm);
		forth_parse(ctx);
	}
	else
	{
		//*--(ctx->sp) = 0;
		//*--(ctx->sp) = 0;
        forth_PUSH(ctx, 0);
        forth_PUSH(ctx, 0);
	}
}

// ---------------------------------------------------------------------------------------------------------------
static forth_byte_t map_digit(char c)
{
	// ASCII
	if ((c >= '0') && (c <= '9'))
	{
		return c - '0';
	}
	else if ((c >= 'a') && (c <= 'z'))
	{
		return 10 + (c - 'a');
	}
	else if ((c >= 'A') && (c <= 'Z'))
	{
		return 10 + (c - 'A');
	}
	return 255;
}

//#define DEBUG_PROCESS_NUMBER

// ( -- x|0 ) For single
// ( -- xl xh 1) For double
// Returns 0 on success and -1 on failure.
int forth_PROCESS_NUMBER(forth_runtime_context_t *ctx, const char *buff, forth_cell_t len)
{
	int sign = 1;
	int is_double = 0;
	char c;
	forth_dcell_t d = 0;
	forth_byte_t b;
	forth_cell_t base = ctx->base;


#if defined(DEBUG_PROCESS_NUMBER)
	printf("%s: buff = %p %lu\n",__FUNCTION__,buff, len);
    puts("--------------------");
    fflush(stdout);
	ctx->write_string(ctx, buff, len); ctx->send_cr(ctx);
    puts("====================");
#endif

	if ((0 != len) && ('-' == *buff))
	{
		sign = -1;
		buff++;
		len--;
	}
	else if ((0 != len) && ('+' == *buff))
	{
		buff++;
		len--;
	}

	if (0 == len)
	{
		return -1;
	}

#if defined(FORTH_ALLOW_0X_HEX)
	if (len > 2)
	{
		if (('0' == buff[0]) && (('x' == buff[1]) || ('X' == buff[1])))
		{
			base = 16;
			len -= 2;
			buff += 2;
		}
	}
#endif

	while (0 != len)
	{
		c = *buff++;
		len--;

		if ('.' == c)
		{
			is_double = 1;
			continue;
		}

		b = map_digit(c);

		if (b >= base)
		{
			return -1;
		}

		d = (d * base) + b;
	}

	if (-1 == sign)
	{
		d = (forth_dcell_t)(-(forth_sdcell_t)d);
	}

	*--(ctx->sp) = FORTH_CELL_LOW(d);

	if (is_double)
	{
		*--(ctx->sp) = FORTH_CELL_HIGH(d);
		*--(ctx->sp) = 1;
	}
	else
	{
		*--(ctx->sp) = 0;
	}


#if defined(DEBUG_PROCESS_NUMBER)
	puts("-----------------------------------------------------------");
	forth_dots(ctx);
	puts("-----------------------------------------------------------");
#endif
	return 0;
}
// ---------------------------------------------------------------------------------------------------------------
void forth_interpret(forth_runtime_context_t *ctx)
{
    int res;
    forth_cell_t symbol_len;
    forth_cell_t symbol_addr;
    while(1)
    {
        forth_parse_name(ctx);
        symbol_len  = forth_POP(ctx);
        symbol_addr = forth_POP(ctx);
        if (0 == symbol_len)
        {
            return;
        }
        forth_PUSH(ctx, symbol_addr);
        forth_PUSH(ctx, symbol_len);
        forth_find_name(ctx);
        if (0 != *(ctx->sp))
        {
            forth_execute(ctx);
        }
        else
        {
            (void)forth_POP(ctx);
            res = forth_PROCESS_NUMBER(ctx, (const char *)symbol_addr, symbol_len);
            // forth_dots(ctx);
            if (0 > res)
            {
                forth_THROW(ctx, -13);
            }
            forth_drop(ctx);
        }
    }
}
// Comment ( "string" -- )
void forth_paren(forth_runtime_context_t *ctx)
{
    forth_PUSH(ctx, (forth_cell_t)')');
    forth_parse(ctx);
    forth_drop(ctx);
    forth_drop(ctx);
}

// .( ( "string" -- )
void forth_dot_paren(forth_runtime_context_t *ctx)
{
    forth_PUSH(ctx, (forth_cell_t)')');
    forth_parse(ctx);
    forth_type(ctx);
}
// ---------------------------------------------------------------------------------------------------------------
void forth_PRINT_ERROR(forth_runtime_context_t *ctx, forth_scell_t code)
{
    
	if ((0 == code) || (1 == code))
	{
		return;
	}

	if ((-2 == code) && (0 == ctx->abort_msg_len))
	{
		return;
	}

#if defined(FORTH_INCLUDE_FILE_ACCESS_WORDS)
// If needed port code here from EFCI, but currently not supported.
#endif

	forth_DOT(ctx, 10, ctx->to_in);
	forth_TYPE0(ctx, " Error: ");
	forth_DOT(ctx, 10, code);

	switch(code)
	{
	case 0: break; // Do nothing for 0.
	case -1: forth_TYPE0(ctx, "ABORT"); break;
	case -2:
		if ((0 != ctx->abort_msg_len) && (0 != ctx->abort_msg_addr))
		{
			ctx->write_string(ctx, (char *)(ctx->abort_msg_addr), ctx->abort_msg_len);
			ctx->abort_msg_addr = 0;
			ctx->abort_msg_len = 0;
		}
		else
		{
			forth_TYPE0(ctx, "aborted");
		}
		break;
	case -3: forth_TYPE0(ctx, "stack overflow"); break;
	case -4: forth_TYPE0(ctx, "stack underflow"); break;
	case -5: forth_TYPE0(ctx, "return stack overflow"); break;
	case -6: forth_TYPE0(ctx, "return stack underflow"); break;
	case -7: forth_TYPE0(ctx, "do-loops nested too deeply during execution"); break;
	case -8: forth_TYPE0(ctx, "dictionary overflow"); break;
	case -9: forth_TYPE0(ctx, "invalid memory address"); break;
	case -10: forth_TYPE0(ctx, "division by zero"); break;
	case -11: forth_TYPE0(ctx, "result out of range"); break;
	case -12: forth_TYPE0(ctx, "argument type mismatch"); break;
	case -13: forth_TYPE0(ctx, "undefined word"); break;
	case -14: forth_TYPE0(ctx, "interpreting a compile-only word"); break;
	case -15: forth_TYPE0(ctx, "invalid FORGET"); break;
	case -16: forth_TYPE0(ctx, "attempt to use zero-length string as a name"); break;
	case -17: forth_TYPE0(ctx, "pictured numeric output string overflow"); break;
	case -18: forth_TYPE0(ctx, "parsed string overflow"); break;
	case -19: forth_TYPE0(ctx, "definition name too long"); break;
	case -20: forth_TYPE0(ctx, "write to a read-only location"); break;
	case -21: forth_TYPE0(ctx, "unsupported operation"); break; // (e.g., AT-XY on a too-dumb terminal)
	case -22: forth_TYPE0(ctx, "control structure mismatch"); break;
	case -23: forth_TYPE0(ctx, "address alignment exception"); break;
	case -24: forth_TYPE0(ctx, "invalid numeric argument"); break;
	case -25: forth_TYPE0(ctx, "return stack imbalance"); break;
	case -26: forth_TYPE0(ctx, "loop parameters unavailable"); break;
	case -27: forth_TYPE0(ctx, "invalid recursion"); break;
	case -28: forth_TYPE0(ctx, "user interrupt"); break;
	case -29: forth_TYPE0(ctx, "compiler nesting"); break;
	case -30: forth_TYPE0(ctx, "obsolescent feature"); break;
	case -31: forth_TYPE0(ctx, ">BODY used on non-CREATEd definition"); break;
	case -32: forth_TYPE0(ctx, "invalid name argument (e.g., TO xxx)"); break;
	case -33: forth_TYPE0(ctx, "block read exception"); break;
	case -34: forth_TYPE0(ctx, "block write exception"); break;
	case -35: forth_TYPE0(ctx, "invalid block number"); break;
	case -36: forth_TYPE0(ctx, "invalid file position"); break;
	case -37: forth_TYPE0(ctx, "file I/O exception"); break;
	case -38: forth_TYPE0(ctx, "non-existent file"); break;
	case -39: forth_TYPE0(ctx, "unexpected end of file"); break;
	case -40: forth_TYPE0(ctx, "invalid BASE for floating point conversion"); break;
	case -41: forth_TYPE0(ctx, "loss of precision"); break;
	case -42: forth_TYPE0(ctx, "floating-point divide by zero"); break;
	case -43: forth_TYPE0(ctx, "floating-point result out of range"); break;
	case -44: forth_TYPE0(ctx, "floating-point stack overflow"); break;
	case -45: forth_TYPE0(ctx, "floating-point stack underflow"); break;
	case -46: forth_TYPE0(ctx, "floating-point invalid argument"); break;
	case -47: forth_TYPE0(ctx, "compilation word list deleted"); break;
	case -48: forth_TYPE0(ctx, "invalid POSTPONE"); break;
	case -49: forth_TYPE0(ctx, "search-order overflow"); break;
	case -50: forth_TYPE0(ctx, "search-order underflow"); break;
	case -51: forth_TYPE0(ctx, "compilation word list changed"); break;
	case -52: forth_TYPE0(ctx, "control-flow stack overflow"); break;
	case -53: forth_TYPE0(ctx, "exception stack overflow"); break;
	case -54: forth_TYPE0(ctx, "floating-point underflow"); break;
	case -55: forth_TYPE0(ctx, "floating-point unidentified fault"); break;
	case -56: forth_TYPE0(ctx, "QUIT"); break;
	case -57: forth_TYPE0(ctx, "exception in sending or receiving a character"); break;
	case -58: forth_TYPE0(ctx, "[IF], [ELSE], or [THEN] exception"); break;
	default: break;
	}

	ctx->send_cr(ctx);
}

// .ERROR ( err -- )
void forth_print_error(forth_runtime_context_t *ctx)
{
    forth_PRINT_ERROR(ctx, forth_POP(ctx));
}

// ---------------------------------------------------------------------------------------------------------------
// BYE ( -- ) has to leave the Forth system and return to the OS or whoever has called it.
// Since we do not know how many layers of threaded code or C code is on the call stack, or how many CATCH-es
// are in between the only real option to unwind all layers of both Forth and C is to do a long jump to the outermost function
// inside the Forth subsystem (which is usually forth()) and let it return to its caller.
// That will transfer control outside the forth system, and properly unwind stuff on C's stack as well.
//
void forth_bye(forth_runtime_context_t *ctx)
{
    jmp_buf *handler = (jmp_buf *)(ctx->bye_handler);

    if (0 == handler)
    {
        forth_THROW(ctx, -21); // Unsupported operation -- any better idea???
    }
    
    longjmp(*handler, -1);
}
// ---------------------------------------------------------------------------------------------------------------
forth_scell_t forth_RUN_INTERPRET(forth_runtime_context_t *ctx)
{
    forth_scell_t res;
    
    // We are discarding the 'const' qualifier from the pointer
    // but our options are limited,
    //see FIND-NAME for details.
    res = forth_CATCH(ctx, (forth_xt_t)&forth_interpret_xt);
    forth_PRINT_ERROR(ctx, res);
    return res;
}

// QUIT ( -- ) ( R: i * x -- )
void forth_quit(forth_runtime_context_t *ctx)
{
    jmp_buf *handler;
    jmp_buf frame;

    if (0 != ctx->quit_handler)
    {
        handler = (jmp_buf *)(ctx->quit_handler);
        longjmp(*handler, -1);
    }

    if (0 == setjmp(frame))
    {
        ctx->quit_handler = (forth_ucell_t)(&frame);
    }

    ctx->rp = ctx->rp0;
    ctx->throw_handler = 0;
    ctx->source_id = 0;
    ctx->state = 0;

    while(1)
    {
        if (0 == ctx->state)
        {
            forth_TYPE0(ctx, "OK");
            forth_cr(ctx);
        }

        forth_refill(ctx);

        if (0 == forth_POP(ctx))
        {
            break;
        }

        forth_RUN_INTERPRET(ctx);
    }

    forth_bye(ctx);
}
// ---------------------------------------------------------------------------------------------------------------
// NOOP ( -- )
void forth_noop(forth_runtime_context_t *ctx)
{

}

// DECIMAL ( -- )
void forth_decimal(forth_runtime_context_t *ctx)
{
	ctx->base = 10;
}

// HEX ( -- )
void forth_hex(forth_runtime_context_t *ctx)
{
	ctx->base = 16;
}

// BASE ( -- addr )
void forth_base(forth_runtime_context_t *ctx)
{
	forth_PUSH(ctx, (forth_cell_t) &(ctx->base));
}

// STATE ( -- addr )
void forth_state(forth_runtime_context_t *ctx)
{
	forth_PUSH(ctx, (forth_cell_t) &(ctx->state));
}

// ---------------------------------------------------------------------------------------------------------------
//                                          Live Compiler and Threaded Code Execution
// ---------------------------------------------------------------------------------------------------------------
forth_dictionary_t *forth_INIT_DICTIONARY(void *addr, forth_cell_t length)
{
	forth_dictionary_t *dict;

	if ((0 == addr) || (length < sizeof(forth_dictionary_t)))
	{
		return (forth_dictionary_t *)0;
	}

	memset(addr, 0, length);

	dict = (forth_dictionary_t *)addr;
	dict->dp = 0;
	length -= FORTH_ALIGN(sizeof(forth_dictionary_t));
	dict->dp_max = length;
}

// Interpreter for threaded code.
void forth_InnerInterpreter(forth_runtime_context_t *ctx, forth_xt_t xt)
{
	forth_xt_t *saved_ip = ctx->ip;

	ctx->ip = (forth_xt_t *) &(xt->meaning);

	while (0 != *(ctx->ip))
	{
		forth_EXECUTE(ctx, *(ctx->ip++));
	}
}

// Details of how constants are implemented.
void forth_DoConst(forth_runtime_context_t *ctx, forth_xt_t xt)
{
	forth_PUSH(ctx, xt->meaning);
}

// Details of how variables are implemented.
void forth_DoVar(forth_runtime_context_t *ctx, forth_xt_t xt)
{
	forth_PUSH(ctx, (forth_cell_t) &(xt->meaning));
}

// HERE ( -- addr )
void forth_here(forth_runtime_context_t *ctx)
{
	if (0 == ctx->dictionary)
	{
		forth_THROW(ctx, -21); // Unsupported opration.
	}

	forth_PUSH(ctx, (forth_cell_t)&(ctx->dictionary->items[ctx->dictionary->dp]));
}

// UNUSED ( -- u )
void forth_unused(forth_runtime_context_t *ctx)
{
	if (0 == ctx->dictionary)
	{
		forth_THROW(ctx, -21); // Unsupported opration.
	}

	forth_PUSH(ctx, ctx->dictionary->dp_max - ctx->dictionary->dp);
}

// ALLOT ( n -- )
void forth_allot(forth_runtime_context_t *ctx)
{
	forth_cell_t n = forth_POP(ctx);
	forth_cell_t dp;

	if (0 == ctx->dictionary)
	{
		forth_THROW(ctx, -21); // Unsupported opration.
	}

	dp = n + ctx->dictionary->dp;

	if (dp > ctx->dictionary->dp_max)
	{
		forth_THROW(ctx, -8); // Dictionary overflow.
	}

	ctx->dictionary->dp = dp;
}

// ALIGN ( -- )
void forth_align(forth_runtime_context_t *ctx)
{
	forth_cell_t dp;

	if (0 == ctx->dictionary)
	{
		forth_THROW(ctx, -21); // Unsupported opration.
	}

	dp = ctx->dictionary->dp;
	dp = FORTH_ALIGN(dp);

	if (dp > ctx->dictionary->dp_max)
	{
		forth_THROW(ctx, -8); // Dictionary overflow.
	}

	ctx->dictionary->dp = dp;
}

// ALIGNED ( addr -- a-addr )
void forth_aligned(forth_runtime_context_t *ctx)
{
	forth_cell_t addr = forth_POP(ctx);
	addr = FORTH_ALIGN(addr);
	forth_PUSH(ctx, addr);
}

// C, ( c -- )
void forth_c_comma(forth_runtime_context_t *ctx)
{
	forth_cell_t chr = forth_POP(ctx);
	forth_cell_t dp;

	if (0 == ctx->dictionary)
	{
		forth_THROW(ctx, -21); // Unsupported opration.
	}

	dp = ctx->dictionary->dp;
	ctx->dictionary->items[dp++] = (uint8_t)chr;

	if (dp > ctx->dictionary->dp_max)
	{
		forth_THROW(ctx, -8); // Dictionary overflow.
	}

	ctx->dictionary->dp = dp;
}


void forth_COMMA(forth_runtime_context_t *ctx, forth_cell_t x)
{
	forth_cell_t ix;
	forth_cell_t dp;

	if (0 == ctx->dictionary)
	{
		forth_THROW(ctx, -21); // Unsupported opration.
	}

	ix = ctx->dictionary->dp;

	if (ix != (ix & FORTH_ALIGNED_MASK))
	{
		forth_THROW(ctx, -23);	// Address alignment exception.
	}

	*(forth_cell_t *)&(ctx->dictionary->items[ix]) = x;
	dp = ix + sizeof(forth_cell_t);

	if (dp > ctx->dictionary->dp_max)
	{
		forth_THROW(ctx, -8); // Dictionary overflow.
	}

	ctx->dictionary->dp = dp;
}

// , ( u -- )
void forth_comma(forth_runtime_context_t *ctx)
{
	forth_cell_t x = forth_POP(ctx);
	forth_COMMA(ctx, x);
}

forth_vocabulary_entry_t *forth_CREATE_DICTIONARY_ENTRY(forth_runtime_context_t *ctx)
{
	const char *name;
	forth_cell_t name_length;
	forth_cell_t len;
	forth_vocabulary_entry_t *res;
	uint8_t *here;

	if (0 == ctx->dictionary)
	{
		forth_THROW(ctx, -21); // Unsupported opration.
	}

	name_length = forth_POP(ctx);
	name = (const char *)forth_POP(ctx);
	len = name_length + 1;

	if ((sizeof(forth_vocabulary_entry_t) + len) > (ctx->dictionary->dp_max - ctx->dictionary->dp))
	{
		forth_THROW(ctx, -8); // Dictionary overflow.
	}

	forth_here(ctx);
	here = (uint8_t *)forth_POP(ctx);
	forth_PUSH(ctx, len);
	forth_allot(ctx);
	memmove(here, name, name_length);
	here[name_length] = 0;
	forth_align(ctx);
	forth_here(ctx);
	res = (forth_vocabulary_entry_t *)forth_POP(ctx);
	forth_COMMA(ctx, (forth_cell_t)here);				// name
	forth_COMMA(ctx, 0);								// flags
	forth_COMMA(ctx, ctx->dictionary->latest);			// link
	forth_COMMA(ctx, 0);			// place holder for meaning
	return res;
}

void forth_variable(forth_runtime_context_t *ctx)
{
	forth_vocabulary_entry_t *entry;
	//const char *name;
	//forth_cell_t len;
	forth_parse_name(ctx);
	//len = forth_POP(ctx);
	//name = (const char *)forth_POP(ctx);
	entry = forth_CREATE_DICTIONARY_ENTRY(ctx);
	entry->flags = FORTH_XT_FLAGS_ACTION_VARIABLE;
	entry->link = ctx->dictionary->latest;
	ctx->dictionary->latest = (forth_cell_t)entry;
}
// ---------------------------------------------------------------------------------------------------------------
// HELP ( -- )
void forth_help(forth_runtime_context_t *ctx)
{
	const forth_vocabulary_entry_t **wl;// = forth_master_list_of_lists;
    const forth_vocabulary_entry_t *ep;

   	for (wl = forth_master_list_of_lists; 0 != *wl; wl++)
    {
    	for (ep = *wl; 0 != ep->name; ep++)
    	{
        	forth_TYPE0(ctx, (char *)(ep->name));
#if !defined(FORTH_EXCLUDE_DESCRIPTIONS)
        	if (0 != ep->link)
        	{
            	forth_EMIT(ctx, '\t');
            	forth_TYPE0(ctx, (char *)(ep->link));
        	}
#endif
        	forth_cr(ctx);
    	}
	}
}

void forth_words(forth_runtime_context_t *ctx)
{
    size_t len;
   	const forth_vocabulary_entry_t *ep;
    const forth_vocabulary_entry_t **wl = forth_master_list_of_lists;

	if (0 != ctx->dictionary)
	{

		for (ep = (forth_vocabulary_entry_t *)(ctx->dictionary->latest); 0 != ep; ep = (forth_vocabulary_entry_t *)(ep->link))
		{
			len = strlen((char *)(ep->name));

			if ((ctx->terminal_width - ctx->terminal_col) <= len)
			{
            	forth_cr(ctx);
        	}

        	forth_TYPE0(ctx, (char *)(ep->name));
        	forth_space(ctx);
			//ep = (forth_vocabulary_entry_t *)(ep->link);
		}
	}

    for (wl = forth_master_list_of_lists; 0 != *wl; wl++)
    {
    	for (ep = *wl; 0 != ep->name; ep++)
    	{
        	len = strlen((char *)(ep->name));

        	if ((ctx->terminal_width - ctx->terminal_col) <= len)
			{
            	forth_cr(ctx);
        	}

        	forth_TYPE0(ctx, (char *)(ep->name));
        	forth_space(ctx);
    	}
	}

    forth_cr(ctx);
}

void forth_tick(forth_runtime_context_t *ctx)
{
    forth_parse_name(ctx);
    forth_find_name(ctx);
    if (0 == ctx->sp[0])
    {
        forth_THROW(ctx, -13);
    }
}

const forth_vocabulary_entry_t forth_wl_forth[] =
{
DEF_FORTH_WORD("quit",       0, forth_quit,          "( -- )"),
DEF_FORTH_WORD( "bye",        0, forth_bye,           "( -- )"),

DEF_FORTH_WORD("(",          0, forth_paren,         " ( -- )"),
DEF_FORTH_WORD(".(",         0, forth_dot_paren,     " ( -- )"),

DEF_FORTH_WORD("execute",    0, forth_execute,       "( xt -- )"),
DEF_FORTH_WORD("catch",      0, forth_catch,         "( xt -- code )"),
DEF_FORTH_WORD("throw",      0, forth_throw,         "( code -- )"),
DEF_FORTH_WORD("depth",      0, forth_depth,         "( -- depth )"),
DEF_FORTH_WORD("dup",        0, forth_dup,           "( x -- x x )"),
DEF_FORTH_WORD("drop",       0, forth_drop,          "( x -- x )"),
DEF_FORTH_WORD("swap",       0, forth_swap,          "( x y -- y x )"),
DEF_FORTH_WORD("over",       0, forth_over,          "( x y -- x y x )"),
DEF_FORTH_WORD("@",          0, forth_fetch,          "( addr -- val )"),
DEF_FORTH_WORD("!",          0, forth_store,          "( val addr -- )"),
DEF_FORTH_WORD("2dup",       0, forth_2dup,          "( x y -- x y x y )"),
DEF_FORTH_WORD("2drop",      0, forth_2drop,         "( x y -- )"),
DEF_FORTH_WORD("2swap",      0, forth_2swap,         "( x y a b -- a b x y )"),
DEF_FORTH_WORD("2over",      0, forth_2over,         "( x y a b -- x y a b x y )"),
DEF_FORTH_WORD("+",          0, forth_add,           "( x y -- x+y )"),
DEF_FORTH_WORD("-",          0, forth_subtract,      "( x y -- x-y )"),
DEF_FORTH_WORD("*",          0, forth_multiply,      "( x y -- x*y )"),
DEF_FORTH_WORD("/",          0, forth_divide,        "( x y -- x/y )"),
DEF_FORTH_WORD("mod",        0, forth_mod,           "( x y -- x%y )"),
DEF_FORTH_WORD("and",        0, forth_and,           "( x y -- x&y )"),
DEF_FORTH_WORD("or",         0, forth_or,            "( x y -- x|y )"),
DEF_FORTH_WORD("xor",        0, forth_xor,           "( x y -- x^y )"),

DEF_FORTH_WORD("type",       0, forth_type,          "( addr count -- )"),
DEF_FORTH_WORD("space",      0, forth_space,         "( -- )" ),
DEF_FORTH_WORD("spaces",     0, forth_spaces,        "( n -- )"),
DEF_FORTH_WORD("emit",       0, forth_emit,          "( char -- )"),
DEF_FORTH_WORD("cr",         0, forth_cr,            "( -- )"),

DEF_FORTH_WORD(".",          0, forth_dot,           "( x -- )"),
DEF_FORTH_WORD("h.",         0, forth_hdot,          "( x -- )"),
DEF_FORTH_WORD("u.",         0, forth_udot,          "( x -- )"),
DEF_FORTH_WORD(".s",         0, forth_dots,          "( -- )"),
DEF_FORTH_WORD("dump",       0, forth_dump,          "( addr count -- )"),

DEF_FORTH_WORD("key?",       0, forth_key_q,         "( -- flag )"),
DEF_FORTH_WORD("key",        0, forth_key,           "( -- key )"),
DEF_FORTH_WORD("ekey?",      0, forth_ekey_q,        "( -- flag )"),
DEF_FORTH_WORD("ekey",       0, forth_ekey,          "( -- key-event )" ),
DEF_FORTH_WORD("ekey>char",  0, forth_ekey2char,     "( key-event -- key-event false | char true )"),
DEF_FORTH_WORD("accept",     0, forth_accept,        "( c-addr len1 -- len2 )"),
DEF_FORTH_WORD("refill",     0, forth_refill,        "( -- flag )"),

DEF_FORTH_WORD("parse",      0, forth_parse,         "( char -- c-addr len )"),
DEF_FORTH_WORD("parse-name", 0, forth_parse_name,    "( \"name\" -- c-addr len )"),
DEF_FORTH_WORD("find-name",  0, forth_find_name,     "( c-addr len -- xt|0)"),
DEF_FORTH_WORD("'",          0, forth_tick,          "( \"name\" -- xt )"),
DEF_FORTH_WORD("interpret",  0, forth_interpret,     "( -- )" ),
DEF_FORTH_WORD(".error",     0, forth_print_error,   "( error_code -- )"),
DEF_FORTH_WORD("noop",       0, forth_noop,          "( -- )"),
DEF_FORTH_WORD("decimal",    0, forth_decimal,       "( -- )"),
DEF_FORTH_WORD("hex",    	 0, forth_hex,       	 "( -- )"),
DEF_FORTH_WORD("base",       0, forth_base,          "( -- addr )"),

DEF_FORTH_WORD("state",      0, forth_state,         "( -- addr )"),
DEF_FORTH_WORD("here",       0, forth_here,          "( -- addr )"),
DEF_FORTH_WORD("unused",     0, forth_unused,        "( -- u )"),
DEF_FORTH_WORD("aligned",    0, forth_aligned,       "( addr -- a-addr )"),
DEF_FORTH_WORD("align",      0, forth_align,         "( --  )"),
DEF_FORTH_WORD("allot",      0, forth_allot,       	 "( n --  )"),
DEF_FORTH_WORD("c,",      	 0, forth_c_comma,       "( c --  )"),
DEF_FORTH_WORD(",",      	 0, forth_comma,         "( x --  )"),
DEF_FORTH_WORD("compile,",   0, forth_comma,         "( xt --  )"),
DEF_FORTH_WORD("variable",   0, forth_variable,       "( \"name\" --)"),
DEF_FORTH_WORD("1", FORTH_XT_FLAGS_ACTION_CONSTANT, 1, "One"),
DEF_FORTH_WORD("0", FORTH_XT_FLAGS_ACTION_CONSTANT, 0, "Zero"),
DEF_FORTH_WORD("true", FORTH_XT_FLAGS_ACTION_CONSTANT, ~0, 0),
DEF_FORTH_WORD("false", FORTH_XT_FLAGS_ACTION_CONSTANT, 0, 0),
DEF_FORTH_WORD("bl", FORTH_XT_FLAGS_ACTION_CONSTANT, FORTH_CHAR_SPACE, "( -- space )"),
DEF_FORTH_WORD("words",      0, forth_words,         "( -- )"),
DEF_FORTH_WORD("help",       0, forth_help,          "( -- )"),

DEF_FORTH_WORD(0, 0, 0, 0)
};

const forth_vocabulary_entry_t forth_interpret_xt =     DEF_FORTH_WORD("INTERPRET",  0, forth_interpret,      "( -- )" );

// Interpret the text in CMD.
// The command is passed as address and length (so we can interpret substrings inside some bigger buffer).
// A flag is passed to indicate if the data stack in the context needs to be emtied before running the command.
//
// This function returns 0 on success and a non-zero value (which is a code from CATCH/THROW) if an error has occured.
int forth(forth_runtime_context_t *ctx, const char *cmd, unsigned int cmd_length, int clear_stack)
{
    forth_cell_t res;
    jmp_buf frame;

    if (0 == cmd_length)
    {
        return 0;
    }

    if (0 == cmd)
    {
        return -9; // Invalid memory address, is there anything better here?
    }

    ctx->bye_handler = 0;
    ctx->quit_handler = 0;
    ctx->throw_handler = 0;

    if (0 != setjmp(frame))
    {
        return 0;
    }

    ctx->bye_handler = (forth_ucell_t)(&frame);

	ctx->ip = 0;
    ctx->rp = ctx->rp0;

    if (clear_stack)
    {
        ctx->sp = ctx->sp0;
    }

#if 0
    cmd = " 1 2 3 4 xxx";
    cmd_length = strlen(cmd);
#endif
    ctx->to_in = 0;
    ctx->source_address = cmd;
    ctx->source_length = cmd_length;
	ctx->source_id = -1;
	ctx->blk = 0;

    res = forth_RUN_INTERPRET(ctx);

    // forth_DOTS(ctx);

    return (int)res;
}
