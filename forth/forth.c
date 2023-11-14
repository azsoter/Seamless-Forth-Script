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

//#include <stdio.h>

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

        longjmp(*handler, (int)code);
    }
}

// Check if the stack contains at least n items.
void forth_CHECK_STACK_AT_LEAST(forth_runtime_context_t *ctx, forth_cell_t n)
{
	if ((ctx->sp + n) > ctx->sp_max)
    {
        forth_THROW(ctx, -4);
    }
}

// Push an item to the data stack, perform stack checking.
void forth_PUSH(forth_runtime_context_t *ctx, forth_ucell_t x)
{
    ctx->sp -= 1;
    if (ctx->sp < ctx->sp_min)
    {
        forth_THROW(ctx, -3);  // Stack overflow.
    }
    *(ctx->sp) = x;
}

// Pop an item from the data stack, perform stack checking.
forth_cell_t forth_POP(forth_runtime_context_t *ctx)
{
    forth_cell_t x = *(ctx->sp++);

    if (ctx->sp > ctx->sp_max)
    {
        forth_THROW(ctx, -4); // Stack underflow.
    }
    return x;
}

// Read double on the top of the forth stack, but don't pop it.
forth_dcell_t forth_DTOS_READ(forth_runtime_context_t *ctx)
{
	forth_dcell_t dtos;
	forth_CHECK_STACK_AT_LEAST(ctx, 2);
	dtos = FORTH_DCELL(ctx->sp[0], ctx->sp[1]);
	return dtos;
}

// Pop a double from the data stack.
forth_dcell_t forth_DPOP(forth_runtime_context_t *ctx)
{
		forth_dcell_t dtos = forth_DTOS_READ(ctx);
		ctx->sp += 2;
		return dtos;
}

// Push a double onto the data stack.
void forth_DPUSH(forth_runtime_context_t *ctx, forth_dcell_t ud)
{
	forth_PUSH(ctx, FORTH_CELL_LOW(ud));
	forth_PUSH(ctx, FORTH_CELL_HIGH(ud));
}

// Push an item to the return stack, perform stack checking.
void forth_RPUSH(forth_runtime_context_t *ctx, forth_ucell_t x)
{
    ctx->rp -= 1;
    if (ctx->rp < ctx->rp_min)
    {
        forth_THROW(ctx, -5); // Return stack overflow.
    }
    *(ctx->rp) = x;
}

// Pop an item from the return stack, perform stack checking.
forth_cell_t forth_RPOP(forth_runtime_context_t *ctx)
{
    forth_cell_t x = *(ctx->rp++);

    if (ctx->rp > ctx->rp_max)
    {
        forth_THROW(ctx, -6);  // Return stack underflow.
    }
    return x;
}

void forth_EXECUTE(forth_runtime_context_t *ctx, forth_xt_t xt)
{
    if (0 == xt)
    {
        forth_THROW(ctx, -13); // Is there a better value to throw here???????
    }

	if (ctx->trace)
	{
		forth_PRINT_TRACE(ctx, xt);
	}

	if (ctx->user_break) // Used has pressed break.
	{
		ctx->user_break = 0; // Delete the indicator.
		forth_THROW(ctx, -28); // User interrupt. 
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

		case FORTH_XT_FLAGS_ACTION_CREATE:
			forth_DoCreate(ctx, xt);
			break;

		case FORTH_XT_FLAGS_ACTION_DEFER:
			forth_DoDefer(ctx, xt);
			break;

		default:
			forth_THROW(ctx, -21); // unsupported operation
			break;
	}
}

// EXECUTE ( xt -- )
void forth_execute(forth_runtime_context_t *ctx)
{
    forth_xt_t xt = (forth_xt_t)forth_POP(ctx);
	forth_EXECUTE(ctx, xt);
}

// Interpreter for threaded code.
void forth_InnerInterpreter(forth_runtime_context_t *ctx, forth_xt_t xt)
{
	forth_xt_t x;

	forth_RPUSH(ctx, (forth_cell_t)ctx->ip);

	ctx->ip = &(xt->meaning);

	while (0 != *(ctx->ip))
	{
		x = (forth_xt_t)*(ctx->ip++);
		forth_EXECUTE(ctx, x);
	}

	ctx->ip = (forth_cell_t *)forth_RPOP(ctx);
}

#if !defined(FORTH_WITHOUT_COMPILATION)
// EXIT ( -- )
// Given the implementation of the inner interpreter (above) in order to exit some threaded code
// we just need to point ctx->ip to a cell that is set to 0.
// So this implementation will work.
void forth_exit(forth_runtime_context_t *ctx)
{
	static const forth_cell_t the_end = 0;
	ctx->ip = (forth_cell_t *)&the_end;
}
#endif

// (TRACE) ( -- addr )
void forth_paren_trace(forth_runtime_context_t *ctx)
{
	forth_PUSH(ctx, (forth_cell_t) &(ctx->trace));
}

// TRACE-ON ( -- )
void forth_trace_on(forth_runtime_context_t *ctx)
{
	ctx->trace = FORTH_TRUE;
}

// TRACE-OFF ( -- )
void forth_trace_off(forth_runtime_context_t *ctx)
{
	ctx->trace = FORTH_FALSE;
}

// ABORT ( -- )
void forth_abort(forth_runtime_context_t *ctx)
{
    forth_THROW(ctx, -1);
}

#if !defined(FORTH_WITHOUT_COMPILATION)
// (ABORT") ( flag c-addr len -- )
// A factor of ABORT"
void forth_pabortq(forth_runtime_context_t *ctx)
{
	forth_cell_t len = forth_POP(ctx);
	forth_cell_t addr = forth_POP(ctx);
	forth_cell_t f = forth_POP(ctx);

	if (0 != f)
	{
		ctx->abort_msg_addr = addr;
		ctx->abort_msg_len = len;
    	forth_THROW(ctx, -2);
	}
}

// ABORT" ( flag -- )
void forth_abort_quote(forth_runtime_context_t *ctx)
{
	forth_squot(ctx);
	forth_COMPILE_COMMA(ctx, forth_pABORTq_xt);
}
#endif

// THROW ( code|0 -- )
void forth_throw(forth_runtime_context_t *ctx)
{
    forth_scell_t code = (forth_scell_t)forth_POP(ctx);
    forth_THROW(ctx, code);
}

#if 0
// CATCH ( xt -- code|0 )
void forth_catch(forth_runtime_context_t *ctx)
{
    int res;
    forth_ucell_t *saved_sp = ctx->sp;
    forth_ucell_t *saved_rp = ctx->rp;
    forth_ucell_t saved_handler = ctx->throw_handler;
	forth_cell_t *saved_ip = ctx->ip;
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
#else
// This version of CATCH relies more heavily on Forth's return stack, which is bounds checked.
// The previous version stores everything on C's stack, but on a resource-limited system that can also
// overflow and even crash something for real.
// By moving stuff to Forth's return stack we are more likely to hit a limit there (unless things are badly misconfigured)
// and just get an exception inside the Forth system instead of a segmentation fault.
//
// CATCH ( xt -- code|0 )
void forth_catch(forth_runtime_context_t *ctx)
{
    int res;
    forth_ucell_t *saved_rp;
    jmp_buf catch_frame;

    if ((ctx->rp - 3) < ctx->rp_min)
    {
		ctx->sp[0] = -53; 	// exception stack overflow
		return;
    }

	ctx->rp -= 3;
	saved_rp = ctx->rp;
	ctx->rp[0] = ctx->throw_handler;
	ctx->rp[1] = (forth_cell_t)ctx->sp;
	ctx->rp[2] = (forth_cell_t)ctx->ip;

    res = setjmp(catch_frame);

    if (0 == res)
    {
//		ctx->ip = 0;
        ctx->throw_handler = (forth_ucell_t)(&catch_frame);
        forth_execute(ctx);
        forth_PUSH(ctx, 0);
    }
    else
    {
		ctx->rp = saved_rp;
        ctx->sp = (forth_cell_t *)ctx->rp[1];
		ctx->ip = (forth_cell_t *)ctx->rp[2];
        *(ctx->sp) = res;
    }

    ctx->throw_handler = ctx->rp[0];
	ctx->rp += 3;
}
#endif

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

// SP@ ( -- sp )
void forth_sp_fetch(forth_runtime_context_t *ctx)
{
	forth_PUSH(ctx, (forth_cell_t)(ctx->sp));
}

// SP! ( sp -- )
void forth_sp_store(forth_runtime_context_t *ctx)
{
	forth_cell_t *sp = (forth_cell_t *)forth_POP(ctx);


	if (sp < ctx->sp_min)
    {
        forth_THROW(ctx, -3);  // Stack overflow.
    }

	if (sp > ctx->sp_max)
    {
        forth_THROW(ctx, -4); // Stack underflow.
    }

	ctx->sp = sp;
}

// SP0 ( -- sp0 )
void forth_sp0(forth_runtime_context_t *ctx)
{
	forth_PUSH(ctx, (forth_cell_t)(ctx->sp0));
}

// RP@ ( -- rp )
void forth_rp_fetch(forth_runtime_context_t *ctx)
{
	forth_PUSH(ctx, (forth_cell_t)(ctx->rp));
}

// RP! ( rp -- )
void forth_rp_store(forth_runtime_context_t *ctx)
{
	forth_cell_t *rp = (forth_cell_t *)forth_POP(ctx);


	if (rp < ctx->rp_min)
    {
        forth_THROW(ctx, -5); // Return stack overflow.
    }

	if (rp > ctx->rp_max)
    {
        forth_THROW(ctx, -6);  // Return stack underflow.
    }

	ctx->rp = rp;
}

// RP0 ( -- rp0 )
void forth_rp0(forth_runtime_context_t *ctx)
{
	forth_PUSH(ctx, (forth_cell_t)(ctx->rp0));
}

// DEPTH ( -- depth )
void forth_depth(forth_runtime_context_t *ctx)
{
    forth_PUSH(ctx, (forth_cell_t)(ctx->sp0 - ctx->sp));
}

// DUP ( x -- x x )
void forth_dup(forth_runtime_context_t *ctx)
{
	forth_CHECK_STACK_AT_LEAST(ctx, 1);
    forth_PUSH(ctx, ctx->sp[0]);
}

// ?DUP ( 0|x -- 0|x x )
void forth_question_dup(forth_runtime_context_t *ctx)
{
	forth_CHECK_STACK_AT_LEAST(ctx, 1);
	if (0 != ctx->sp[0])
	{
    	forth_PUSH(ctx, ctx->sp[0]);
	}
}

// DROP ( x -- )
void forth_drop(forth_runtime_context_t *ctx)
{
    (void)forth_POP(ctx);
}

// NIP ( x y -- y )
void forth_nip(forth_runtime_context_t *ctx)
{
	forth_cell_t c = forth_POP(ctx);
    (void)forth_POP(ctx);
	forth_PUSH(ctx, c);
}

// TUCK ( x y -- y x y )
void forth_tuck(forth_runtime_context_t *ctx)
{
	forth_cell_t y = forth_POP(ctx);
	forth_cell_t x = forth_POP(ctx);
	forth_PUSH(ctx, y);
	forth_PUSH(ctx, x);
	forth_PUSH(ctx, y);
}


// -ROT ( x y z -- z x y )
void forth_mrot(forth_runtime_context_t *ctx)
{
	forth_cell_t z = forth_POP(ctx);
	forth_cell_t y = forth_POP(ctx);
	forth_cell_t x = forth_POP(ctx);
	forth_PUSH(ctx, z);
	forth_PUSH(ctx, x);
	forth_PUSH(ctx, y);
}

// ROT ( x y z -- y z x )
void forth_rot(forth_runtime_context_t *ctx)
{
	forth_cell_t z = forth_POP(ctx);
	forth_cell_t y = forth_POP(ctx);
	forth_cell_t x = forth_POP(ctx);
	forth_PUSH(ctx, y);
	forth_PUSH(ctx, z);
	forth_PUSH(ctx, x);
}

// PICK ( xu..x1 x0 u --  xu..x1 x0 xu)
void forth_pick(forth_runtime_context_t *ctx)
{
	forth_cell_t ix = forth_POP(ctx);
	forth_CHECK_STACK_AT_LEAST(ctx, (int)ix); // **TODO** This may not be OK!!!!
	forth_PUSH(ctx, ctx->sp[ix]);
}

// CS-PICK ( C: destu ... orig0 | dest0 -- destu ... orig0 | dest0 destu ) ( S: u -- )
void forth_cspick(forth_runtime_context_t *ctx)
{
	forth_cell_t ix = forth_POP(ctx);
	forth_CHECK_STACK_AT_LEAST(ctx, (2*ix + 2));
	forth_PUSH(ctx, ctx->sp[2*ix + 1]);
	forth_PUSH(ctx, ctx->sp[2*ix + 1]);
}

// ROLL ( xu xu-1 ... x0 u -- xu-1 ... x0 xu )
void forth_roll(forth_runtime_context_t *ctx)
{
	forth_cell_t i = forth_POP(ctx);
	forth_cell_t tos;

	forth_CHECK_STACK_AT_LEAST(ctx, i);

	if (0 != i)
	{
		tos = ctx->sp[i];

		while(i)
		{
			ctx->sp[i] = ctx->sp[i - 1];
			i--;
		}
		ctx->sp[0] = tos;
	}
}

// CS-ROLL ( C: origu | destu origu-1 | destu-1 ... orig0 | dest0 -- origu-1 | destu-1 ... orig0 | dest0 origu | destu ) ( S: u -- )
void forth_csroll(forth_runtime_context_t *ctx)
{
	forth_cell_t ix = forth_POP(ctx);
	forth_PUSH(ctx, 2*ix + 1);
	forth_roll(ctx);
	forth_PUSH(ctx, 2*ix + 1);
	forth_roll(ctx);
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

// +! ( val addr - )
void forth_plus_store(forth_runtime_context_t *ctx)
{
	forth_cell_t *p = (forth_cell_t *)forth_POP(ctx);
	forth_cell_t  c = forth_POP(ctx);
	*p += c;
}

// C@ ( addr - val )
void forth_cfetch(forth_runtime_context_t *ctx)
{
	uint8_t *p = (uint8_t *)forth_POP(ctx);
	uint8_t  c = *p;
	forth_PUSH(ctx, (forth_cell_t)c);
}

// C! ( val addr - )
void forth_cstore(forth_runtime_context_t *ctx)
{
	uint8_t *p = (uint8_t *)forth_POP(ctx);
	uint8_t  c = (uint8_t)forth_POP(ctx);
	*p = c;
}

// ? ( addr - )
void forth_questionmark(forth_runtime_context_t *ctx)
{
	forth_fetch(ctx);
	forth_dot(ctx);
}

// INVERT ( x -- ~x )
void forth_invert(forth_runtime_context_t *ctx)
{
	forth_CHECK_STACK_AT_LEAST(ctx, 1);
	ctx->sp[0] = ~(ctx->sp[0]);
}

// NEGATE ( x -- -x )
void forth_negate(forth_runtime_context_t *ctx)
{
	forth_CHECK_STACK_AT_LEAST(ctx, 1);
	ctx->sp[0] = (forth_cell_t)(-1*(forth_scell_t)ctx->sp[0]);
}

// ABS ( x -- |x| )
void forth_abs(forth_runtime_context_t *ctx)
{
	forth_CHECK_STACK_AT_LEAST(ctx, 1);
	if (0 > (forth_scell_t)(ctx->sp[0]))
	{
		ctx->sp[0] = (forth_cell_t)(-1*(forth_scell_t)ctx->sp[0]);
	}
}

// 2* ( x -- 2*x )
void forth_2mul(forth_runtime_context_t *ctx)
{
	forth_CHECK_STACK_AT_LEAST(ctx, 1);
	ctx->sp[0] = 2*ctx->sp[0];
}

// 2/ ( x -- x/2 )
void forth_2div(forth_runtime_context_t *ctx)
{
	forth_CHECK_STACK_AT_LEAST(ctx, 1);
	ctx->sp[0] = (((forth_scell_t)ctx->sp[0]) >> 1);
}

// LSHIFT ( x1 sh -- x2 )
void forth_lshift(forth_runtime_context_t *ctx)
{
	forth_cell_t sh = forth_POP(ctx);

	forth_CHECK_STACK_AT_LEAST(ctx, 1);

	ctx->sp[0] <<= sh;
}

// RSHIFT ( x1 sh -- x2 )
void forth_rshift(forth_runtime_context_t *ctx)
{
	forth_cell_t sh = forth_POP(ctx);

	forth_CHECK_STACK_AT_LEAST(ctx, 1);

	ctx->sp[0] >>= sh;
}

// M* ( n1 n2  -- d )
void forth_m_mult(forth_runtime_context_t *ctx)
{
	forth_scell_t n2 = (forth_scell_t)forth_POP(ctx);
	forth_scell_t n1 = (forth_scell_t)forth_POP(ctx);
	forth_sdcell_t d = ((forth_sdcell_t)n1) * n2;
	forth_DPUSH(ctx, (forth_dcell_t)d);
}

// UM* ( u1 u2  -- ud )
void forth_um_mult(forth_runtime_context_t *ctx)
{
	forth_cell_t u2 = forth_POP(ctx);
	forth_cell_t u1 = forth_POP(ctx);
	forth_dcell_t ud = ((forth_dcell_t)u1) * u2;
	forth_DPUSH(ctx, ud);
}

// 1+ ( x -- x+1 )
void forth_1plus(forth_runtime_context_t *ctx)
{
	forth_CHECK_STACK_AT_LEAST(ctx, 1);
	ctx->sp[0] += 1;
}

// 1- ( x -- x-1 )
void forth_1minus(forth_runtime_context_t *ctx)
{
	forth_CHECK_STACK_AT_LEAST(ctx, 1);
	ctx->sp[0] -= 1;
}

// CELL+ ( x -- y )
void forth_cell_plus(forth_runtime_context_t *ctx)
{
	forth_CHECK_STACK_AT_LEAST(ctx, 1);
	ctx->sp[0] += sizeof(forth_cell_t);
}

// CELLS ( x -- y )
void forth_cells(forth_runtime_context_t *ctx)
{
	forth_CHECK_STACK_AT_LEAST(ctx, 1);
	ctx->sp[0] *= sizeof(forth_cell_t);
}

// ---------------------------------------------------------------------------------------------------------------
//                                                Two item and double operations
// ---------------------------------------------------------------------------------------------------------------
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

// 2ROT ( x1 x2 x3 x4 x5 x6 -- x3 x4 x5 x6 x1 x2 )
void forth_2rot(forth_runtime_context_t *ctx)
{
	// In this system 2 CS-ROLL has the correct semantics....
	forth_PUSH(ctx, 2);
	forth_csroll(ctx);
}

// 2@ ( addr - x y )
void forth_2fetch(forth_runtime_context_t *ctx)
{
	forth_cell_t *p = (forth_cell_t *)forth_POP(ctx);
	forth_PUSH(ctx, p[1]);
	forth_PUSH(ctx, p[0]);
}

// 2! ( x y addr - )
void forth_2store(forth_runtime_context_t *ctx)
{
	forth_cell_t *p = (forth_cell_t *)forth_POP(ctx);
	p[0] = forth_POP(ctx);
	p[1] = forth_POP(ctx);
}

#if !defined(FORTH_NO_DOUBLES)
// D0< ( d -- flag )
void forth_dzero_less(forth_runtime_context_t *ctx)
{
	forth_sdcell_t dtos = (forth_sdcell_t)forth_DPOP(ctx);
	forth_PUSH(ctx, (dtos < 0) ? FORTH_TRUE : FORTH_FALSE);
}

// D0< ( d -- flag )
void forth_dzero_equals(forth_runtime_context_t *ctx)
{
	forth_sdcell_t dtos = (forth_sdcell_t)forth_DPOP(ctx);
	forth_PUSH(ctx, (0 == dtos) ? FORTH_TRUE : FORTH_FALSE);
}

// D< ( d1 d2 -- flag )
void forth_dless(forth_runtime_context_t *ctx)
{
	forth_sdcell_t d2 = (forth_sdcell_t)forth_DPOP(ctx);
	forth_sdcell_t d1 = (forth_sdcell_t)forth_DPOP(ctx);
	forth_PUSH(ctx, (d1 < d2) ? FORTH_TRUE : FORTH_FALSE);
}

// DU< ( du1 du2 -- flag )
void forth_duless(forth_runtime_context_t *ctx)
{
	forth_udcell_t d2 = forth_DPOP(ctx);
	forth_udcell_t d1 = forth_DPOP(ctx);
	forth_PUSH(ctx, (d1 < d2) ? FORTH_TRUE : FORTH_FALSE);
}

// D= ( d1 d2 -- flag )
void forth_dequals(forth_runtime_context_t *ctx)
{
	forth_sdcell_t d2 = (forth_sdcell_t)forth_DPOP(ctx);
	forth_sdcell_t d1 = (forth_sdcell_t)forth_DPOP(ctx);
	forth_PUSH(ctx, (d1 == d2) ? FORTH_TRUE : FORTH_FALSE);
}

// D+ ( d1 d2 -- d )
void forth_dplus(forth_runtime_context_t *ctx)
{
	forth_dcell_t d2 = forth_DPOP(ctx);
	forth_dcell_t d1 = forth_DPOP(ctx);
	forth_DPUSH(ctx, d1+d2);
}

// D- ( d1 d2 -- d )
void forth_dminus(forth_runtime_context_t *ctx)
{
	forth_dcell_t d2 = forth_DPOP(ctx);
	forth_dcell_t d1 = forth_DPOP(ctx);
	forth_DPUSH(ctx, d1-d2);
}

// M+ ( d1 n -- d )
void forth_mplus(forth_runtime_context_t *ctx)
{
	forth_scell_t   n = (forth_scell_t)forth_POP(ctx);
	forth_sdcell_t d1 = (forth_sdcell_t)forth_DPOP(ctx);
	forth_DPUSH(ctx, (forth_dcell_t)(d1+n));
}

// DNEGATE ( d -- -d )
void forth_dnegate(forth_runtime_context_t *ctx)
{
	forth_sdcell_t dtos = (forth_sdcell_t)forth_DPOP(ctx);
	dtos = -dtos;
	forth_DPUSH(ctx, (forth_dcell_t)dtos);
}

// DABS ( d -- |d| )
void forth_dabs(forth_runtime_context_t *ctx)
{
	forth_CHECK_STACK_AT_LEAST(ctx, 2);
	if (0 > (forth_scell_t)ctx->sp[0])
	{
		forth_dnegate(ctx);
	}
}

// DMIN ( d1 d2 -- d )
void forth_dmin(forth_runtime_context_t *ctx)
{
	forth_2over(ctx);
	forth_2over(ctx);
	forth_dless(ctx);
	if (0 == forth_POP(ctx))
	{
		forth_2swap(ctx);
	}
	forth_2drop(ctx);
}

// DMAX ( d1 d2 -- d )
void forth_dmax(forth_runtime_context_t *ctx)
{
	forth_2over(ctx);
	forth_2over(ctx);
	forth_dless(ctx);
	if (0 != forth_POP(ctx))
	{
		forth_2swap(ctx);
	}
	forth_2drop(ctx);
}

// S>D ( s -- d )
void forth_s_to_d(forth_runtime_context_t *ctx)
{
	forth_dup(ctx);
	forth_zero_less(ctx);
}

// 2* ( d -- d*2 )
void forth_d2mul(forth_runtime_context_t *ctx)
{
	forth_scell_t tos = (forth_scell_t)forth_DPOP(ctx);
	tos = tos << 1;
	forth_DPUSH(ctx, tos);
}

// 2/ ( d -- d/2 )
void forth_d2div(forth_runtime_context_t *ctx)
{
	forth_scell_t tos = (forth_scell_t)forth_DPOP(ctx);
	tos = tos >> 1;
	forth_DPUSH(ctx, tos);
}

// D. ( d -- )
void forth_ddot(forth_runtime_context_t *ctx)
{
	forth_scell_t x;
	forth_CHECK_STACK_AT_LEAST(ctx, 2);
	x = (forth_scell_t)ctx->sp[0];
	forth_dabs(ctx);
	forth_less_hash(ctx);
	forth_PUSH(ctx, FORTH_CHAR_SPACE);
	forth_hold(ctx);
	forth_hash_s(ctx);
	forth_PUSH(ctx, (forth_cell_t)x);
	forth_sign(ctx);
	forth_hash_greater(ctx);
	forth_type(ctx);
}
#endif // !defined(FORTH_NO_DOUBLES)
// ---------------------------------------------------------------------------------------------------------------
//                                                Return stack operations
// ---------------------------------------------------------------------------------------------------------------
// >R ( x -- ) R: ( -- x )
void forth_to_r(forth_runtime_context_t *ctx)
{
	forth_RPUSH(ctx, forth_POP(ctx));
}

// R@ ( -- x ) R: ( x -- x )
void forth_r_fetch(forth_runtime_context_t *ctx)
{
	if ((ctx->rp + 1) > ctx->rp_max)
    {
        forth_THROW(ctx, -6);
    }

	forth_PUSH(ctx, ctx->rp[0]);
}

// R> ( -- x ) R: ( x -- )
void forth_r_from(forth_runtime_context_t *ctx)
{
	forth_PUSH(ctx, forth_RPOP(ctx));
}

// 2>R ( x y -- ) R: ( -- x y)
void forth_2to_r(forth_runtime_context_t *ctx)
{
	forth_cell_t y = forth_POP(ctx);
	forth_cell_t x = forth_POP(ctx);
	forth_RPUSH(ctx, x);
	forth_RPUSH(ctx, y);
}

// 2R@ ( -- x y ) R: ( x y -- x y )
void forth_2r_fetch(forth_runtime_context_t *ctx)
{
	if ((ctx->rp + 2) > ctx->rp_max)
    {
        forth_THROW(ctx, -6);
    }

	forth_PUSH(ctx, ctx->rp[1]);
	forth_PUSH(ctx, ctx->rp[0]);
}

// 2R> ( -- x y ) R: ( x y -- )
void forth_2r_from(forth_runtime_context_t *ctx)
{
	forth_cell_t y = forth_RPOP(ctx);
	forth_cell_t x = forth_RPOP(ctx);
	forth_PUSH(ctx, x);
	forth_PUSH(ctx, y);
}

// N>R ( i*n n -- ) R: ( -- i*n n )
void forth_n_to_r(forth_runtime_context_t *ctx)
{
	forth_cell_t n = forth_POP(ctx);
	forth_cell_t i;
	for (i = 0; i < n; i++)
	{
		forth_RPUSH(ctx, forth_POP(ctx));
	}
	forth_RPUSH(ctx, n);
}

// NR> ( -- i*n n ) R: ( i*n n -- )
void forth_n_r_from(forth_runtime_context_t *ctx)
{
	forth_cell_t n = forth_RPOP(ctx);
	forth_cell_t i;
	for (i = 0; i < n; i++)
	{
		forth_PUSH(ctx, forth_RPOP(ctx));
	}
	forth_PUSH(ctx, n);
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

// */mod ( x y z -- x*y%z x*y/z )
void forth_mult_div_mod(forth_runtime_context_t *ctx)
{
	forth_scell_t z = (forth_scell_t )forth_POP(ctx);
    forth_scell_t y = (forth_scell_t )forth_POP(ctx);
    forth_scell_t x = (forth_scell_t )forth_POP(ctx);
	forth_sdcell_t d = (forth_sdcell_t)x * (forth_sdcell_t)y;

    forth_PUSH(ctx, (forth_cell_t)(d%z));
    forth_PUSH(ctx, (forth_cell_t)(d/z));
}

// */ ( x y z -- x*y/z )
void forth_mult_div(forth_runtime_context_t *ctx)
{
	forth_mult_div_mod(ctx);
	forth_nip(ctx);
}

// UM/MOD ( ud u -- m q )
void forth_um_div_mod(forth_runtime_context_t *ctx)
{
	forth_cell_t u = forth_POP(ctx);
	forth_dcell_t ud = forth_DPOP(ctx);

	forth_PUSH(ctx, (forth_cell_t)(ud % u));
	forth_PUSH(ctx, (forth_cell_t)(ud / u));
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

// /MOD ( x y -- x%y x/y )
void forth_div_mod(forth_runtime_context_t *ctx)
{
    forth_scell_t y = (forth_scell_t)forth_POP(ctx);
    forth_scell_t x = (forth_scell_t)forth_POP(ctx);

    if (0 == y)
    {
        forth_THROW(ctx, -10);
    }

    forth_PUSH(ctx, (forth_cell_t)(x%y));
	forth_PUSH(ctx, (forth_cell_t)(x/y));
}

// MIN ( x y -- x|y )
void forth_min(forth_runtime_context_t *ctx)
{
    forth_scell_t y = (forth_scell_t)forth_POP(ctx);
    forth_scell_t x = (forth_scell_t)forth_POP(ctx);
    
    forth_PUSH(ctx, (forth_cell_t)((x < y) ? x : y));
}

// Based on the reference implementation
// : WITHIN ( test low high -- flag ) OVER - >R - R> U< ;
void forth_within(forth_runtime_context_t *ctx)
{
	forth_cell_t tmp;
	forth_over(ctx);
	forth_subtract(ctx);
	tmp = forth_POP(ctx);
	forth_subtract(ctx);
	forth_PUSH(ctx, tmp);
	forth_uless(ctx);
}

// MAX ( x y -- x|y )
void forth_max(forth_runtime_context_t *ctx)
{
    forth_scell_t y = (forth_scell_t)forth_POP(ctx);
    forth_scell_t x = (forth_scell_t)forth_POP(ctx);
    
    forth_PUSH(ctx, (forth_cell_t)((x > y) ? x : y));
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

// = ( x y -- flag )
void forth_equals(forth_runtime_context_t *ctx)
{
	forth_cell_t y = forth_POP(ctx);
	forth_cell_t x = forth_POP(ctx);
	forth_PUSH(ctx, (x == y) ? FORTH_TRUE: FORTH_FALSE);
}

// <> ( x y -- flag )
void forth_not_equals(forth_runtime_context_t *ctx)
{
	forth_cell_t y = forth_POP(ctx);
	forth_cell_t x = forth_POP(ctx);
	forth_PUSH(ctx, (x != y) ? FORTH_TRUE: FORTH_FALSE);
}

// u< ( x y -- flag )
void forth_uless(forth_runtime_context_t *ctx)
{
	forth_cell_t y = forth_POP(ctx);
	forth_cell_t x = forth_POP(ctx);
	forth_PUSH(ctx, (x < y) ? FORTH_TRUE: FORTH_FALSE);
}

// u> ( x y -- flag )
void forth_ugreater(forth_runtime_context_t *ctx)
{
	forth_cell_t y = forth_POP(ctx);
	forth_cell_t x = forth_POP(ctx);
	forth_PUSH(ctx, (x > y) ? FORTH_TRUE: FORTH_FALSE);
}

// < ( x y -- flag )
void forth_less(forth_runtime_context_t *ctx)
{
	forth_scell_t y = (forth_scell_t)forth_POP(ctx);
	forth_scell_t x = (forth_scell_t)forth_POP(ctx);
	forth_PUSH(ctx, (x < y) ? FORTH_TRUE: FORTH_FALSE);
}

// > ( x y -- flag )
void forth_greater(forth_runtime_context_t *ctx)
{
	forth_scell_t y = (forth_scell_t)forth_POP(ctx);
	forth_scell_t x = (forth_scell_t)forth_POP(ctx);
	forth_PUSH(ctx, (x > y) ? FORTH_TRUE: FORTH_FALSE);
}

// 0= ( x -- flag )
void forth_zero_equals(forth_runtime_context_t *ctx)
{
	forth_cell_t x = forth_POP(ctx);
	forth_PUSH(ctx, (0 == x) ? FORTH_TRUE: FORTH_FALSE);
}

// 0<> ( x -- flag )
void forth_zero_not_equals(forth_runtime_context_t *ctx)
{
	forth_cell_t x = forth_POP(ctx);
	forth_PUSH(ctx, (0 != x) ? FORTH_TRUE: FORTH_FALSE);
}

// 0< ( x -- flag )
void forth_zero_less(forth_runtime_context_t *ctx)
{
	forth_scell_t x = (forth_scell_t)forth_POP(ctx);
	forth_PUSH(ctx, (x < 0) ? FORTH_TRUE: FORTH_FALSE);
}

// 0> ( x -- flag )
void forth_zero_greater(forth_runtime_context_t *ctx)
{
	forth_scell_t x = (forth_scell_t)forth_POP(ctx);
	forth_PUSH(ctx, (x > 0) ? FORTH_TRUE: FORTH_FALSE);
}

// ---------------------------------------------------------------------------------------------------------------
//                                                  String/Memory Operations
// ---------------------------------------------------------------------------------------------------------------
// FILL ( c-addr len char -- )
void forth_fill(forth_runtime_context_t *ctx)
{
	char c = (char)forth_POP(ctx);
	forth_cell_t len = forth_POP(ctx);
	char *addr = (char *)forth_POP(ctx);

	memset(addr, c, len);
}

// ERASE ( c-addr len -- )
void forth_erase(forth_runtime_context_t *ctx)
{
	forth_PUSH(ctx, 0);
	forth_fill(ctx);
}

// BLANK ( c-addr len -- )
void forth_blank(forth_runtime_context_t *ctx)
{
	forth_PUSH(ctx, FORTH_CHAR_SPACE);
	forth_fill(ctx);
}

// MOVE ( src dest len -- )
void forth_move(forth_runtime_context_t *ctx)
{
	forth_cell_t len = forth_POP(ctx);
	void *dst_addr = (void *)forth_POP(ctx);
	void *src_addr = (void *)forth_POP(ctx);

	if (len > 0)
	{
		(void)memmove(dst_addr, src_addr, (size_t)len);
	}
}

forth_scell_t forth_COMPARE_STRINGS(const char *s1, forth_cell_t len1, const char *s2, forth_cell_t len2)
{

	while(1)
	{
		if ((0 == len1) && (0 == len2))	// No characters left -- the strings are the same.
		{
			return 0;
		}
		else if (0 == len1) // Only the first string has ended
		{
			return -1;
		}
		else if (0 == len2) // Only the second string has ended
		{
			return 1;
		}

		if (*s1 != *s2)
		{
			return (0 > ( ((signed char)*s1) - ((signed char)*s2) )) ? -1 : 1;
		}

		len1--;
		s1++;
		len2--;
		s2++;
	}
}

// COMPARE ( c-addr1 u1 c-addr2 u2 -- n )
void forth_compare(forth_runtime_context_t *ctx)
{
	forth_cell_t len2 = forth_POP(ctx);
	const char *s2 = (const char *)forth_POP(ctx);
	forth_cell_t len1 = forth_POP(ctx);
	const char *s1 = (const char *)forth_POP(ctx);
	forth_PUSH(ctx, (forth_cell_t)forth_COMPARE_STRINGS(s1, len1, s2, len2));
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
	(void)res;
    /* Just ignore errors here for now........
	if (0 > res)
    {
        forth_THROW(ctx, -57);
    }
	*/
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

// AT-XY (x y  -- )
void forth_at_xy(forth_runtime_context_t *ctx)
{
	forth_cell_t y = forth_POP(ctx);
	forth_cell_t x = forth_POP(ctx);
	int res;

	if (0 == ctx->at_xy)
	{
		forth_THROW(ctx, -21);
	}

    res = ctx->at_xy(ctx, x, y);

    if (0 > res)
    {
        forth_THROW(ctx, -57);
    }
}

// PAGE ( -- )
void forth_page(forth_runtime_context_t *ctx)
{
	int res;

	if (0 == ctx->page)
	{
		forth_THROW(ctx, -21);
	}

    res = ctx->page(ctx);

    if (0 > res)
    {
        forth_THROW(ctx, -57);
    }
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
#if defined(FORTH_INCLUDE_BLOCKS)
	if (0 != ctx->blk)
	{
		if ((ctx->blk + 1) >= (FORTH_MAX_BLOCKS)) // We are at the last block, cannot refill.
		{
			forth_PUSH(ctx, FORTH_FALSE);
		}
		else
		{
			forth_ADJUST_BLK_INPUT_SOURCE(ctx, ctx->blk + 1);
			ctx->blk += 1;
			ctx->to_in = 0;
			forth_PUSH(ctx, FORTH_TRUE);
		}
		return;
	}
#endif
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

int forth_HDOT(forth_runtime_context_t *ctx, forth_cell_t value)
{
	char *buffer = ctx->num_buff;
	char *end = buffer + (FORTH_CELL_HEX_DIGITS) + 1;
	char *p;
	*end = FORTH_CHAR_SPACE;
	p = forth_FORMAT_UNSIGNED(value, 16, FORTH_CELL_HEX_DIGITS, end);
	return ctx->write_string(ctx, p, (end - p) + 1);
}

int forth_UDOT(forth_runtime_context_t *ctx, forth_cell_t base, forth_cell_t value)
{
	char *buffer = ctx->num_buff;
	char *end = buffer + sizeof(ctx->num_buff);
	char *p;
	*end = FORTH_CHAR_SPACE;
	p = forth_FORMAT_UNSIGNED(value, base, 1, end);
	return ctx->write_string(ctx, p, (end - p) + 1);
}

int forth_DOT(forth_runtime_context_t *ctx, forth_cell_t base, forth_cell_t value)
{
	char *buffer = ctx->num_buff;
	char *end = buffer + sizeof(ctx->num_buff);
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

int forth_DOT_R(forth_runtime_context_t *ctx, forth_cell_t base, forth_cell_t value, forth_cell_t width, forth_cell_t is_signed)
{
	char *buffer = ctx->num_buff;
	char *end = buffer + sizeof(ctx->num_buff);
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

static void forth_check_numbuff(forth_runtime_context_t *ctx)
{
	if (ctx->numbuff_ptr < ctx->num_buff)
	{
		forth_THROW(ctx, -17); // pictured numeric output string overflow
	}
}

// <# ( -- )
void forth_less_hash(forth_runtime_context_t *ctx)
{
	ctx->numbuff_ptr = &(ctx->num_buff[FORTH_NUM_BUFF_LENGTH]);
}

// HOLD ( char -- )
void forth_hold(forth_runtime_context_t *ctx)
{
	*(--(ctx->numbuff_ptr)) = (char)forth_POP(ctx);
	forth_check_numbuff(ctx);
}

// HOLDS ( c-addr u -- )
void forth_holds(forth_runtime_context_t *ctx)
{
	forth_ucell_t cnt = forth_POP(ctx);

	while (cnt--)
	{
		forth_count(ctx);	// Cheating, but the implementation has the right behavior.
		forth_hold(ctx);
	}

	forth_drop(ctx);
}

// SIGN ( n -- )
void forth_sign(forth_runtime_context_t *ctx)
{
	forth_scell_t n = (forth_scell_t)forth_POP(ctx);
	if (n < 0)
	{
		forth_PUSH(ctx, (forth_cell_t)'-');
		forth_hold(ctx);
	}
}

// # ( ud1 -- ud2 )
void forth_hash(forth_runtime_context_t *ctx)
{
	forth_udcell_t dtos;
	forth_cell_t base = ctx->base;

	if (0 == base)
	{
		forth_THROW(ctx, -10); // division by zero
	}

	dtos = forth_DPOP(ctx);
	*(--(ctx->numbuff_ptr)) = forth_VAL2DIGIT((forth_byte_t)(dtos % base));
	forth_check_numbuff(ctx);
	dtos = dtos / base;
	forth_DPUSH(ctx, dtos);
}

// #s ( ud1 -- ud2 )
void forth_hash_s(forth_runtime_context_t *ctx)
{
	forth_CHECK_STACK_AT_LEAST(ctx, 2);
	do {

		forth_hash(ctx);
	} while ((0 != ctx->sp[0]) || (0 != ctx->sp[1]));
}

// #> ( ud -- c-addr u )
void forth_hash_greater(forth_runtime_context_t *ctx)
{
	forth_CHECK_STACK_AT_LEAST(ctx, 2);
	ctx->sp[1] = (forth_cell_t)(ctx->numbuff_ptr);
	ctx->sp[0] = (forth_cell_t)(&(ctx->num_buff[FORTH_NUM_BUFF_LENGTH]) - ctx->numbuff_ptr);
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
void forth_PRINT_TRACE(forth_runtime_context_t *ctx, forth_xt_t xt)
{
	if (0 != xt)
	{
		if (0 != xt->name)
		{
			if (0 > forth_HDOT(ctx, (forth_cell_t)(ctx->ip)))
			{
				forth_THROW(ctx, -57);
			}

			forth_TYPE0(ctx, ": ");
			forth_TYPE0(ctx, (const char *)(xt->name));
			forth_space(ctx);
			forth_dots(ctx);
			//forth_cr(ctx);
		}
	}
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

// .R ( n w -- )
void forth_dotr(forth_runtime_context_t *ctx)
{
	int res;
	forth_cell_t width = forth_POP(ctx);
	forth_cell_t value = forth_POP(ctx);

	res = forth_DOT_R(ctx, ctx->base, value, width, 1);

	if (0 > res)
    {
        forth_THROW(ctx, -57);
    }
}

// U.R ( u w -- )
void forth_udotr(forth_runtime_context_t *ctx)
{
	int res;
	forth_cell_t width = forth_POP(ctx);
	forth_cell_t value = forth_POP(ctx);

	res = forth_DOT_R(ctx, ctx->base, value, width, 0);

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
		while ((0 != len) && isspace((int)(*buff)))
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
		while ((0 != len) && !isspace((int)(*buff)))
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

        forth_PUSH(ctx, (forth_cell_t)address);
        forth_PUSH(ctx, length);
	}
	else
	{
        forth_PUSH(ctx, 0);
        forth_PUSH(ctx, 0);
	}
}

// " ( <string> -- c-addr u )"
void forth_quot(forth_runtime_context_t *ctx)
{
	forth_PUSH(ctx,'\"');
	forth_parse(ctx);
}

#if !defined(FORTH_WITHOUT_COMPILATION)
// ( c-addr len -- )
void forth_sliteral(forth_runtime_context_t *ctx)
{
	forth_cell_t len = forth_POP(ctx);
	const char *addr = (const char *)forth_POP(ctx);
	forth_cell_t here;

	forth_COMPILE_COMMA(ctx, forth_SLIT_xt);
	forth_COMMA(ctx, len);
	forth_here(ctx);
	here = forth_POP(ctx);
	forth_PUSH(ctx, len);
	forth_allot(ctx);
	memmove((void *)here, addr, len);
	forth_align(ctx);
}
#endif

// S" ( <string> -- c-addr u )"
void forth_squot(forth_runtime_context_t *ctx)
{
	forth_quot(ctx);

#if !defined(FORTH_WITHOUT_COMPILATION)
	if (0 != ctx->state)
	{
		forth_sliteral(ctx);
	}
#endif
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

// >NUMBER ( ud c-addr len -- ud1 c-addr1 len1 )
void forth_to_number(forth_runtime_context_t *ctx)
{
	forth_cell_t len = forth_POP(ctx);
	const char *p = (const char *)forth_POP(ctx);
	forth_dcell_t dtos = forth_DPOP(ctx);
	forth_cell_t base = ctx->base;
	forth_cell_t digit;

	while(0 != len)
	{
		digit = map_digit(*p);

		if (digit >= base)
		{
			break;
		}

		dtos = (dtos * base) + digit;
		len--;
		p++;
	}

	forth_DPUSH(ctx, dtos);
	forth_PUSH(ctx, (forth_cell_t)p);
	forth_PUSH(ctx, len);
}
// ---------------------------------------------------------------------------------------------------------------
void forth_interpret(forth_runtime_context_t *ctx)
{
    int res;
    forth_cell_t symbol_len;
    forth_cell_t symbol_addr;
	forth_xt_t xt;

    while(1)
    {
        forth_parse_name(ctx);
        symbol_len  = forth_POP(ctx);
        symbol_addr = forth_POP(ctx);

        if (0 == symbol_len)
        {
            return;
        }

		ctx->symbol_addr = symbol_addr;
		ctx->symbol_length = symbol_len;

        forth_PUSH(ctx, symbol_addr);
        forth_PUSH(ctx, symbol_len);
        forth_find_name(ctx);
		xt = (forth_xt_t)forth_POP(ctx);

        if (0 != xt)
        {
#if !defined(FORTH_WITHOUT_COMPILATION)
			if ((0 == ctx->state) || (0 != (FORTH_XT_FLAGS_IMMEDIATE & xt->flags)))
			{
#endif
            	forth_EXECUTE(ctx, xt);
#if !defined(FORTH_WITHOUT_COMPILATION)
			}
			else
			{
				forth_COMPILE_COMMA(ctx, xt);
			}
#endif
        }
        else
        {
            res = forth_PROCESS_NUMBER(ctx, (const char *)symbol_addr, symbol_len);
 
            if (0 > res)
            {
                forth_THROW(ctx, -13); // Undefined word.
            }
#if !defined(FORTH_WITHOUT_COMPILATION)
			if (0 == ctx->state)
			{
#endif
            	forth_drop(ctx);
#if !defined(FORTH_WITHOUT_COMPILATION)
			}
			else
			{
				if (0 == forth_POP(ctx))
				{
					forth_literal(ctx);
				}
				else
				{
					forth_2literal(ctx);
				}
			}
#endif
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

// \ ( -- )
void forth_backslash(forth_runtime_context_t *ctx)
{
#if defined(FORTH_INCLUDE_BLOCKS)
	forth_cell_t in;
	if (0 != ctx->blk)
	{
		in = (ctx->to_in & ~(forth_cell_t)63) + 64;
		ctx->to_in = (in < ctx->source_length) ? in : ctx->source_length;
		return;
	}
#endif
	ctx->to_in = ctx->source_length;
}
// ---------------------------------------------------------------------------------------------------------------
// Exception codes are from here:
// http://lars.nocrew.org/forth2012/exception.html
//
void forth_PRINT_ERROR(forth_runtime_context_t *ctx, forth_scell_t code)
{
#if defined(FORTH_RETRIEVE_SYSTEM_DEFINED_ERROR_MESSAGE)
	const char *msg;
#endif
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

	forth_TYPE0(ctx, " @position: ");
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
	default:
		// If there are locally defined exception codes get the error message here.
		// Function prototype should be defined in forth_config.h.
#if defined(FORTH_RETRIEVE_SYSTEM_DEFINED_ERROR_MESSAGE)

		msg = FORTH_RETRIEVE_SYSTEM_DEFINED_ERROR_MESSAGE(code);

		if (0 != msg)
		{
			forth_TYPE0(ctx, msg);
		}
#endif
		break;
	}

	ctx->send_cr(ctx);
}

// .ERROR ( err -- )
void forth_print_error(forth_runtime_context_t *ctx)
{
    forth_PRINT_ERROR(ctx, forth_POP(ctx));
}

// .VERSION ( -- )
void forth_print_version(forth_runtime_context_t *ctx)
{
	forth_cell_t major   = 0xFF & ((FORTH_ENGINE_VERSION) >> 24);
	forth_cell_t minor   = 0xFF & ((FORTH_ENGINE_VERSION) >> 16);
	forth_cell_t release = 0xFF & ((FORTH_ENGINE_VERSION) >> 8);
	forth_cell_t build   = 0xFF & (FORTH_ENGINE_VERSION);
	forth_cell_t base = ctx->base;

	ctx->base = 10;
	forth_less_hash(ctx);		// <#

	forth_PUSH(ctx, build);
	forth_PUSH(ctx, 0);
	forth_hash_s(ctx);			// #S
	forth_PUSH(ctx, '.');
	forth_hold(ctx);

	ctx->sp[1] = release;
	forth_hash_s(ctx);			// #S
	forth_PUSH(ctx, '.');
	forth_hold(ctx);

	ctx->sp[1] = minor;
	forth_hash_s(ctx);			// #S
	forth_PUSH(ctx, '.');
	forth_hold(ctx);

	ctx->sp[1] = major;
	forth_hash_s(ctx);			// #S

	forth_hash_greater(ctx);	// #>
	forth_type(ctx);
	ctx->base = base;
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
    res = forth_CATCH(ctx, forth_interpret_xt);
	if (0 != res)
	{
		if ((0 != ctx->symbol_addr) && (0 != ctx->symbol_length))
		{
			(void)ctx->write_string(ctx, (const char *)(ctx->symbol_addr), ctx->symbol_length);
		}

    	forth_PRINT_ERROR(ctx, res);
		ctx->state = 0;
		ctx->defining = 0;
	}
    return res;
}

// SAVE-INPUT ( -- blk >in 2 )
void forth_save_input(forth_runtime_context_t *ctx)
{
	forth_PUSH(ctx, ctx->blk);
	forth_PUSH(ctx, ctx->to_in);
	forth_PUSH(ctx, 2);
}

// RESTORE-INPUT ( blk >in 2 -- flag )
void forth_restore_input(forth_runtime_context_t *ctx)
{
	forth_cell_t cnt = forth_POP(ctx);
	forth_cell_t to_in;
	forth_cell_t blk;

	if (2 == cnt)
	{
		// POP first, in case there is a stack underflow, we don't want to do a half restore.
		to_in = forth_POP(ctx);
		blk = forth_POP(ctx);
		ctx->to_in = to_in;
		ctx->blk = blk;
	#if defined(FORTH_INCLUDE_BLOCKS)
		if (0 != blk)
		{
			forth_ADJUST_BLK_INPUT_SOURCE(ctx, blk);
		}
	#endif
		forth_PUSH(ctx, FORTH_FALSE);
	}
	else
	{
		for(; 0 != cnt; cnt--)
		{
			(void)forth_POP(ctx);
		}
		forth_PUSH(ctx, FORTH_TRUE);	// TRUE indicates failure here.....
	}
}

// EVALUATE ( i * x c-addr u -- j * x )
void forth_evaluate(forth_runtime_context_t *ctx)
{
	forth_scell_t res;
	forth_cell_t len = forth_POP(ctx);
	forth_cell_t addr = forth_POP(ctx);
	forth_cell_t saved_source_id = ctx->source_id;
	forth_cell_t saved_blk = ctx->blk;
	forth_cell_t saved_in = ctx->to_in;
	const char *saved_source_address = ctx->source_address;
	forth_cell_t saved_source_length = ctx->source_length;


	if (0 == len)
	{
		return; // Nothing to do.
	}

	if (0 == addr)
	{
		forth_THROW(ctx, -9); // Invalid address.
	}

	ctx->source_id = -1;
	ctx->blk = 0;
	ctx->source_address = (const char *)addr;
	ctx->source_length = len;
	ctx->to_in = 0;

    res = forth_CATCH(ctx, forth_interpret_xt);

	ctx->source_id = saved_source_id;
	ctx->blk = saved_blk;
	ctx->to_in = saved_in;
	ctx->source_address = saved_source_address;
	ctx->source_length = saved_source_length;
#if defined(FORTH_INCLUDE_BLOCKS)
    if (0 != saved_blk)
    {
        forth_ADJUST_BLK_INPUT_SOURCE(ctx, saved_blk);
    }
#endif
	forth_THROW(ctx, res);
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
#if 1
        	// We really do need to bail out if there is no output to write to.
        	// Otherwise we are just going to get looping here when someone throws a -57
        	// because writing is not possible.
        	// This can actually happen if the output can close (such as on a network connection).

        	if ((0 == ctx->write_string) || (0 > ctx->write_string(ctx, "OK", 2)))
        	{
        		break;
        	}

            if ((0 == ctx->send_cr) || (0 > ctx->send_cr(ctx)))
            {
            	break;
            }
#else
            forth_TYPE0(ctx, "OK");
            forth_cr(ctx);
#endif
        }

        forth_refill(ctx);

        if (0 == forth_POP(ctx))
        {
            break;
        }

        if (0 != forth_RUN_INTERPRET(ctx))
		{
			ctx->sp = ctx->sp0;
			ctx->rp = ctx->rp0;
		}
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

// >IN ( -- addr )
void forth_to_in(forth_runtime_context_t *ctx)
{
	forth_PUSH(ctx, (forth_cell_t) &(ctx->to_in));
}

// STATE ( -- addr )
void forth_state(forth_runtime_context_t *ctx)
{
	forth_PUSH(ctx, (forth_cell_t) &(ctx->state));
}

// SOURCE ( -- c-addr len )
void forth_source(forth_runtime_context_t *ctx)
{
	forth_PUSH(ctx, (forth_cell_t)(ctx->source_address));
	forth_PUSH(ctx, ctx->source_length);
}

// SOURCE-ID ( -- id )
void forth_source_id(forth_runtime_context_t *ctx)
{
	forth_PUSH(ctx, ctx->source_id);
}

#if !defined(FORTH_WITHOUT_COMPILATION)
// [ ( -- )
void forth_left_bracket(forth_runtime_context_t *ctx)
{
	ctx->state = 0;
}

// ]  ( -- )
void forth_right_bracket(forth_runtime_context_t *ctx)
{
	ctx->state = FORTH_XT_FLAGS_IMMEDIATE;
}
#endif
// ---------------------------------------------------------------------------------------------------------------
//                                          Live Compiler and Threaded Code Execution
// ---------------------------------------------------------------------------------------------------------------
#if !defined(FORTH_WITHOUT_COMPILATION)
// Initialize a memory area to be used as a Forth Dictionary.
forth_dictionary_t *Forth_InitDictionary(void *addr, forth_cell_t length)
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
	dict->forth_wl.link		= (forth_cell_t)&forth_root_wordlist;
	dict->forth_wl.parent	= (forth_cell_t)&forth_root_wordlist;
	dict->forth_wl.name		= (forth_cell_t)&(dict->items);
	memcpy(dict->items, "Forth", 6);
	dict->dp += FORTH_ALIGN(6);
	dict->last_wordlist = (forth_cell_t)&(dict->forth_wl);

	return dict;
}

// BRANCH ( -- ) Compiled by some words such as ELSE and REPEAT.
void forth_branch(forth_runtime_context_t *ctx)
{
	if (0 == ctx->ip)
	{
		forth_THROW(ctx, -21); // Unsupported operation.
	}

	ctx->ip += (forth_cell_t)(ctx->ip[0]);
}

// 0BRACH ( flag -- ) Compiled by IF, WHILE, UNTIL, etc.
void forth_0branch(forth_runtime_context_t *ctx)
{
	if (0 == ctx->ip)
	{
		forth_THROW(ctx, -21); // Unsupported operation.
	}

	if (0 == forth_POP(ctx))
	{
		ctx->ip += (forth_cell_t)(ctx->ip[0]);
	}
	else
	{
		ctx->ip++;
	}
}

#define FORTH_DO_LOOP_I 0
#define FORTH_DO_LOOP_J 3
#define FORTH_DO_LOOP_LIMIT 1
#define FORTH_DO_LOOP_LEAVE_ADDRESS 2

// (DO) ( limit first -- )
void forth_do_rt(forth_runtime_context_t *ctx)
{
	forth_cell_t *address_after;

	if (0 == ctx->ip)
	{
		forth_THROW(ctx, -21); // Unsupported operation.
	}

	//forth_TYPE0(ctx, "ip ="); forth_PUSH(ctx, (forth_cell_t)ctx->ip); forth_hdot(ctx); forth_cr(ctx);

	address_after = ctx->ip + (forth_cell_t)(ctx->ip[0]);

	//forth_TYPE0(ctx, "address_after="); forth_PUSH(ctx, (forth_cell_t)address_after); forth_hdot(ctx); forth_cr(ctx);

	ctx->ip++;
	ctx->rp -=3;

	if (ctx->rp < ctx->rp_min)
    {
        forth_THROW(ctx, -5); // Return stack overflow.
    }

	ctx->rp[FORTH_DO_LOOP_I] = forth_POP(ctx);
	ctx->rp[FORTH_DO_LOOP_LIMIT] = forth_POP(ctx);
	ctx->rp[FORTH_DO_LOOP_LEAVE_ADDRESS] = (forth_cell_t)address_after;
}

// (?DO) ( limit first -- )
void forth_qdo_rt(forth_runtime_context_t *ctx)
{
	forth_cell_t *address_after;
	forth_cell_t index = forth_POP(ctx);
	forth_cell_t limit = forth_POP(ctx);

	if (0 == ctx->ip)
	{
		forth_THROW(ctx, -21); // Unsupported operation.
	}

	address_after = ctx->ip + (forth_cell_t)(ctx->ip[0]);
	ctx->ip++;

	if (index == limit)
	{
		ctx->ip = address_after;
		return;
	}

	ctx->rp -=3;

	if (ctx->rp < ctx->rp_min)
    {
        forth_THROW(ctx, -5); // Return stack overflow.
    }

	ctx->rp[FORTH_DO_LOOP_I] = index;
	ctx->rp[FORTH_DO_LOOP_LIMIT] = limit;
	ctx->rp[FORTH_DO_LOOP_LEAVE_ADDRESS] = (forth_cell_t)address_after;
}

// ( -- ) R: ( loop-sys -- )
void forth_unloop(forth_runtime_context_t *ctx)
{
	ctx->rp += 3;

	if (ctx->rp > ctx->rp_max)
    {
        forth_THROW(ctx, -26);  // loop parameters unavailable
    }
}

// ( -- ) R: ( loop-sys -- )
void forth_leave(forth_runtime_context_t *ctx)
{
	if ((ctx->rp + 3) > ctx->rp_max)
    {
        forth_THROW(ctx, -26);  // loop parameters unavailable
    }

	ctx->ip = (forth_cell_t *)(ctx->rp[FORTH_DO_LOOP_LEAVE_ADDRESS]);
	ctx->rp += 3;
}

// I ( -- i )
void forth_i(forth_runtime_context_t *ctx)
{
	if ((ctx->rp + 3) > ctx->rp_max)
    {
        forth_THROW(ctx, -26);  // loop parameters unavailable
    }

	forth_PUSH(ctx, ctx->rp[FORTH_DO_LOOP_I]);
}

// J ( -- j )
void forth_j(forth_runtime_context_t *ctx)
{
	if ((ctx->rp + 6) > ctx->rp_max)
    {
        forth_THROW(ctx, -26);  // loop parameters unavailable
    }

	forth_PUSH(ctx, ctx->rp[FORTH_DO_LOOP_J]);
}

// The implementation behind LOOP.
// ( -- ) R: ( loop-sys -- )
void forth_loop_rt(forth_runtime_context_t *ctx)
{
	if (0 == ctx->ip)
	{
		forth_THROW(ctx, -21); // Unsupported operation.
	}

	if ((ctx->rp + 3) > ctx->rp_max)
    {
        forth_THROW(ctx, -6);  // Return stack underflow.
    }

	ctx->rp[FORTH_DO_LOOP_I] += 1;

	if (ctx->rp[FORTH_DO_LOOP_I] == ctx->rp[FORTH_DO_LOOP_LIMIT])
	{
		forth_unloop(ctx);
		ctx->ip++;
	}
	else
	{
		ctx->ip += (forth_cell_t)(ctx->ip[0]);
	}
}

// The implementation behind LOOP.
// ( inc -- ) R: ( loop-sys -- )
void forth_plus_loop_rt(forth_runtime_context_t *ctx)
{
	forth_cell_t inc;
	forth_scell_t tmp;

	if (0 == ctx->ip)
	{
		forth_THROW(ctx, -21); // Unsupported operation.
	}

	if ((ctx->rp + 3) > ctx->rp_max)
    {
        forth_THROW(ctx, -6);  // Return stack underflow.
    }

	inc = forth_POP(ctx);

	ctx->rp[FORTH_DO_LOOP_I] += inc;

 	// Some 2's complement's trickery to determine if the limit has just been crossed.
	tmp = (forth_scell_t)((ctx->rp[FORTH_DO_LOOP_I] - ctx->rp[FORTH_DO_LOOP_LIMIT]) ^ inc);
	if (0 > tmp)
	{
		ctx->ip += (forth_cell_t)(ctx->ip[0]);

	}
	else
	{
		forth_unloop(ctx);
		ctx->ip++;
	}
}

// DO ( limit start -- )
void forth_do(forth_runtime_context_t *ctx)
{
	forth_COMPILE_COMMA(ctx, forth_pDO_xt); // (DO)
	forth_here(ctx);
	forth_COMMA(ctx, 0);
	forth_PUSH(ctx, FORTH_DO_MARKER);
}

// ?DO ( limit start -- )
void forth_q_do(forth_runtime_context_t *ctx)
{
	forth_COMPILE_COMMA(ctx, forth_pqDO_xt); // (?DO)
	forth_here(ctx);
	forth_COMMA(ctx, 0);
	forth_PUSH(ctx, FORTH_DO_MARKER);
}

// LOOP ( -- )
void forth_loop(forth_runtime_context_t *ctx)
{
	forth_cell_t *do_addr;
	forth_cell_t *here;

	if (FORTH_DO_MARKER != forth_POP(ctx))
	{
		forth_THROW(ctx, -22); // control structure mismatch
	}

	forth_COMPILE_COMMA(ctx, forth_pLOOP_xt);
	do_addr = (forth_cell_t *)forth_POP(ctx);
	forth_here(ctx);
	here = (forth_cell_t *)forth_POP(ctx);
	*do_addr = (forth_cell_t)((here + 1) - do_addr);
	forth_COMMA(ctx, (forth_cell_t)((do_addr + 1) - here));
	//printf("here = 0x%16lx\r\n", here);
}

// +LOOP ( n -- )
void forth_plus_loop(forth_runtime_context_t *ctx)
{
	forth_cell_t *do_addr;
	forth_cell_t *here;

	if (FORTH_DO_MARKER != forth_POP(ctx))
	{
		forth_THROW(ctx, -22); // control structure mismatch
	}

	forth_COMPILE_COMMA(ctx, forth_ppLOOP_xt);
	do_addr = (forth_cell_t *)forth_POP(ctx);
	forth_here(ctx);
	here = (forth_cell_t *)forth_POP(ctx);
	*do_addr = (forth_cell_t)((here + 1) - do_addr);
	forth_COMMA(ctx, (forth_cell_t)((do_addr + 1) - here));
}
#endif

// Details of how constants are implemented.
void forth_DoConst(forth_runtime_context_t *ctx, forth_xt_t xt)
{
	forth_PUSH(ctx, xt->meaning);
}

// Details of how DEFER-ed words are implemented.
void forth_DoDefer(forth_runtime_context_t *ctx, forth_xt_t xt)
{
	forth_EXECUTE(ctx, (forth_xt_t)xt->meaning);
}

// Details of how CREATEd words are implemented.
void forth_DoCreate(forth_runtime_context_t *ctx, forth_xt_t xt)
{
	forth_PUSH(ctx, (forth_cell_t)(&(xt->meaning) + 1));

	if (0 != xt->meaning)
	{
		forth_EXECUTE(ctx, (forth_xt_t)(xt->meaning));
	}
}

// Details of how variables are implemented.
void forth_DoVar(forth_runtime_context_t *ctx, forth_xt_t xt)
{
	forth_PUSH(ctx, (forth_cell_t) &(xt->meaning));
}

#if !defined(FORTH_WITHOUT_COMPILATION)
// Compiled by LITERAL
// ( -- n )
void forth_lit(forth_runtime_context_t *ctx)
{
	if (0 == ctx->dictionary)
	{
		forth_THROW(ctx, -21); // Unsupported opration.
	}

	if (0 == ctx->ip)
	{
		forth_THROW(ctx, -9); // Invalid address.
	}

	forth_PUSH(ctx, (forth_cell_t) *(ctx->ip++));
}

// Compiled by SLITERAL
// ( -- c-addr len )
void forth_slit(forth_runtime_context_t *ctx)
{
	forth_cell_t len;
	forth_cell_t ip;

	if (0 == ctx->dictionary)
	{
		forth_THROW(ctx, -21); // Unsupported opration.
	}

	if (0 == ctx->ip)
	{
		forth_THROW(ctx, -9); // Invalid address.
	}

	len = (forth_cell_t) *(ctx->ip++);
	ip = (forth_cell_t)ctx->ip;
	forth_PUSH(ctx, ip);
	forth_PUSH(ctx, len);
	ip += len;
	ctx->ip = (forth_cell_t *)(FORTH_ALIGN(ip));
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
#endif

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

// COUNT ( c-addr -- c-addr+1 c )
void forth_count(forth_runtime_context_t *ctx)
{
	char *p = (char *)forth_POP(ctx);
	char c = *p++;
	forth_PUSH(ctx, (forth_cell_t)p);
	forth_PUSH(ctx, (forth_cell_t) c);
}

#if !defined(FORTH_WITHOUT_COMPILATION)
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

// POSTPONE ( "name" -- )
void forth_postpone(forth_runtime_context_t *ctx)
{
	forth_xt_t xt;

	forth_tick(ctx);
	xt = (forth_xt_t)forth_POP(ctx);

	if (0 != (FORTH_XT_FLAGS_IMMEDIATE & xt->flags)) // Word is immediate.
	{
		forth_COMPILE_COMMA(ctx, xt);
	}
	else
	{
		forth_COMPILE_COMMA(ctx, forth_XLIT_xt);
		forth_COMMA(ctx, (forth_cell_t)xt);
		forth_COMPILE_COMMA(ctx, forth_COMPILE_COMMA_xt);
	}
}

// AHEAD ( -- ) C: ( -- orig )
void forth_ahead(forth_runtime_context_t *ctx)
{
	forth_COMPILE_COMMA(ctx , forth_BRANCH_xt);
	forth_here(ctx);
	forth_COMMA(ctx, 0);
	forth_PUSH(ctx, FORTH_ORIG_MARKER);
}

// IF ( flag -- ) C: ( -- orig )
void forth_if(forth_runtime_context_t *ctx)
{
	forth_COMPILE_COMMA(ctx , forth_0BRANCH_xt);
	forth_here(ctx);
	forth_COMMA(ctx, 0);
	forth_PUSH(ctx, FORTH_ORIG_MARKER);
}

// THEN ( -- ) C: ( orig -- )
void forth_then(forth_runtime_context_t *ctx)
{
	forth_cell_t *p;

	if (FORTH_ORIG_MARKER != forth_POP(ctx))
	{
		forth_THROW(ctx, -22); // Control structure mismatch.
	}

	p = (forth_cell_t *)forth_POP(ctx);
	forth_here(ctx);
	*p = ((forth_cell_t *)forth_POP(ctx)) - p;
}

// ELSE ( -- ) C: ( orig1 -- orig2 )
void forth_else(forth_runtime_context_t *ctx)
{
	forth_COMPILE_COMMA(ctx , forth_BRANCH_xt);
	forth_here(ctx);
	forth_COMMA(ctx, 0);
	forth_mrot(ctx);
	forth_then(ctx);
	forth_PUSH(ctx, FORTH_ORIG_MARKER);
}
#endif

// Based on the reference implementation in the DPANS documents.
// Added exception if refill fails.
// [ELSE] ( -- )
void forth_bracket_else(forth_runtime_context_t *ctx)
{
	forth_cell_t level = 1;
	const char *str;
	forth_cell_t len;
	
	do
	{
		while(1)
		{
			forth_parse_name(ctx);
			len = forth_POP(ctx);
			str = (const char *)forth_POP(ctx);

			if (0 == len)
			{
				break;
			}

			if (forth_COMPARE_NAMES("[IF]", str, len))
			{
				level++;
			} else if (forth_COMPARE_NAMES("[ELSE]", str, len))
			{
				if (0 != --level)
				{
					level++;
				}
			} else if (forth_COMPARE_NAMES("[THEN]", str, len))
			{
				level--;
			}

			if (0 == level)
			{
				return;
			}
		}
		forth_refill(ctx);
	} while (0 != forth_POP(ctx));
	forth_THROW(ctx, -58); // [IF], [ELSE], or [THEN] exception
}

// [IF] ( flag -- )
void forth_bracket_if(forth_runtime_context_t *ctx)
{
	if (0 == forth_POP(ctx))
	{
		forth_bracket_else(ctx);
	}
}

#if !defined(FORTH_WITHOUT_COMPILATION)
// From a suggested 'reference' implementation.
// CASE ( -- ) C: ( -- case-sys )
void forth_case(forth_runtime_context_t *ctx)
{
	forth_PUSH(ctx, 0);
}

// OF ( x1 x2  -- x1 ) C: ( -- of-sys ) ???
void forth_of(forth_runtime_context_t *ctx)
{
	forth_cell_t count = forth_POP(ctx);
	count += 1;
	forth_COMPILE_COMMA(ctx, forth_over_xt);
	forth_COMPILE_COMMA(ctx, forth_equals_xt);
	forth_if(ctx);
	forth_COMPILE_COMMA(ctx, forth_drop_xt);
	forth_PUSH(ctx, count);
}

void forth_endof(forth_runtime_context_t *ctx)
{
	forth_cell_t count = forth_POP(ctx);
	forth_else(ctx);
	forth_PUSH(ctx, count);
}

void forth_endcase(forth_runtime_context_t *ctx)
{
	forth_cell_t count = forth_POP(ctx);
	forth_COMPILE_COMMA(ctx, forth_drop_xt);

	while (count--)
	{
		forth_then(ctx);
	}
}

void forth_dot_quote(forth_runtime_context_t *ctx)
{
	if (0 == ctx->state)
	{
		forth_THROW(ctx, -14); // interpreting a compile-only word
	}
	forth_squot(ctx);
	forth_COMPILE_COMMA(ctx, forth_type_xt);
}

// BEGIN ( -- ) C: ( -- dest )
void forth_begin(forth_runtime_context_t *ctx)
{
	forth_here(ctx);
	forth_PUSH(ctx, FORTH_DEST_MARKER);
}

//  C: ( dest -- )
void forth_BRANCH_TO_DEST(forth_runtime_context_t *ctx, forth_xt_t branch)
{
	forth_cell_t *dest;
	forth_cell_t *here;

	if (FORTH_DEST_MARKER != forth_POP(ctx))
	{
		forth_THROW(ctx, -22); // Control structure mismatch.
	}

	forth_COMPILE_COMMA(ctx, branch);
	dest = (forth_cell_t *)forth_POP(ctx);
	forth_here(ctx);
	here = (forth_cell_t *)forth_POP(ctx);
	forth_COMMA(ctx, (forth_cell_t)(dest - here));
}

// AGAIN ( -- ) C: ( dest -- )
void forth_again(forth_runtime_context_t *ctx)
{
	forth_BRANCH_TO_DEST(ctx, forth_BRANCH_xt);
}

// UNTIL ( f -- ) C: ( dest -- )
void forth_until(forth_runtime_context_t *ctx)
{
	forth_BRANCH_TO_DEST(ctx, forth_0BRANCH_xt);
}

// WHILE ( f -- ) C: ( dest -- orig dest )
void forth_while(forth_runtime_context_t *ctx)
{
	forth_if(ctx);
	forth_2swap(ctx);
}

// REPEAT ( -- ) C: ( orig dest -- )
void forth_repeat(forth_runtime_context_t *ctx)
{
	forth_again(ctx);
	forth_then(ctx);
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

	forth_2dup(ctx);
	forth_find_name(ctx);

	if (0 != forth_POP(ctx))
	{
		forth_cr(ctx);
		forth_TYPE0(ctx, "WARNING: Word ");
		forth_2dup(ctx);
		forth_type(ctx);
		forth_TYPE0(ctx, " is being redefined!");
		forth_cr(ctx);
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
	forth_COMMA(ctx, (forth_cell_t)here);							// name
	forth_COMMA(ctx, 0);											// flags
	forth_COMMA(ctx, (forth_cell_t)forth_GET_LATEST(ctx));			// link

	return res;
}

// LITERAL Compile: ( x -- ) Run: ( -- x ) 
void forth_literal(forth_runtime_context_t *ctx)
{
	forth_COMPILE_COMMA(ctx, forth_LIT_xt);
	forth_comma(ctx);
}

// XLITERAL Compile: ( xt -- ) Run: ( -- xt ) 
void forth_xliteral(forth_runtime_context_t *ctx)
{
	forth_COMPILE_COMMA(ctx, forth_XLIT_xt);
	forth_comma(ctx);
}

// 2LITERAL Compile: ( x y -- ) Run: ( -- x y ) 
void forth_2literal(forth_runtime_context_t *ctx)
{
	forth_swap(ctx);
	forth_COMPILE_COMMA(ctx, forth_LIT_xt);
	forth_comma(ctx);
	forth_COMPILE_COMMA(ctx, forth_LIT_xt);
	forth_comma(ctx);
}

forth_vocabulary_entry_t *forth_PARSE_NAME_AND_CREATE_ENTRY(forth_runtime_context_t *ctx)
{
	forth_parse_name(ctx);

	if (0 == ctx->sp[0])
	{
		forth_THROW(ctx, -16); // Attempt to use zero-length string as a name.
	}

	return forth_CREATE_DICTIONARY_ENTRY(ctx);
}

// VARIABLE ( "name" -- )
void forth_variable(forth_runtime_context_t *ctx)
{
	forth_vocabulary_entry_t *entry;

	entry = forth_PARSE_NAME_AND_CREATE_ENTRY(ctx);
	entry->flags = FORTH_XT_FLAGS_ACTION_VARIABLE;
	forth_COMMA(ctx, 0); // Meaning.
	entry->link = (forth_cell_t)forth_GET_LATEST(ctx);
	forth_SET_LATEST(ctx, entry);
}

// CONSTANT ( value "name" -- )
void forth_constant(forth_runtime_context_t *ctx)
{
	forth_vocabulary_entry_t *entry;
	forth_cell_t value = forth_POP(ctx);

	entry = forth_PARSE_NAME_AND_CREATE_ENTRY(ctx);
	entry->flags = FORTH_XT_FLAGS_ACTION_CONSTANT;
	//entry->meaning = value;
	forth_COMMA(ctx, value); // Meaning.
	entry->link = (forth_cell_t)forth_GET_LATEST(ctx);
	forth_SET_LATEST(ctx, entry);
}

// : ( "name" -- colon-sys)
void forth_colon(forth_runtime_context_t *ctx)
{
	forth_vocabulary_entry_t *entry;

	entry = forth_PARSE_NAME_AND_CREATE_ENTRY(ctx);
	entry->flags = FORTH_XT_FLAGS_ACTION_THREADED;
	forth_PUSH(ctx, (forth_cell_t)entry);
	ctx->defining = ctx->sp[0];
	forth_PUSH(ctx, FORTH_COLON_SYS_MARKER);
	forth_right_bracket(ctx);
}

static const char *nameless = "";
// :NONAME ( -- xt colon-sys )
void forth_colon_noname(forth_runtime_context_t *ctx)
{
	forth_align(ctx);
	forth_here(ctx);	// xt
	forth_COMMA(ctx, (forth_cell_t)nameless);
	forth_COMMA(ctx, FORTH_XT_FLAGS_ACTION_THREADED);	// flags
	forth_COMMA(ctx, 0);								// link (not linked).
	forth_dup(ctx);
	ctx->defining = ctx->sp[0];
	forth_PUSH(ctx, FORTH_COLON_SYS_MARKER);
	forth_right_bracket(ctx);
}

// ; ( colon-sys -- )
void forth_semicolon(forth_runtime_context_t *ctx)
{
	forth_vocabulary_entry_t *entry;

	if (0 == ctx->state)
	{
		forth_THROW(ctx, -14); // Interpreting a compile-only word.
	}

	if (FORTH_COLON_SYS_MARKER != forth_POP(ctx))
	{
		forth_THROW(ctx, -22); // Control structure mismatch.
	}

	forth_COMPILE_COMMA(ctx, 0);
	entry = (forth_vocabulary_entry_t *)forth_POP(ctx);

	if ((0 != entry->name) && (0 != *((const char *)(entry->name)))) // Checking for noname entries.
	{
		entry->link = (forth_cell_t)forth_GET_LATEST(ctx);
		forth_SET_LATEST(ctx, entry);
	}

	ctx->defining = 0;
	forth_left_bracket(ctx);
}

// IMMEDIATE ( -- )
void forth_immediate(forth_runtime_context_t *ctx)
{
	forth_vocabulary_entry_t *entry = forth_GET_LATEST(ctx);
	if (0 != entry)
	{
		entry->flags |= FORTH_XT_FLAGS_IMMEDIATE;
	}
}

forth_vocabulary_entry_t *forth_GET_LATEST(forth_runtime_context_t *ctx)
{
	if (0 == ctx->dictionary)
	{
		forth_THROW(ctx, -21); // 	unsupported operation
	}

	//return (forth_vocabulary_entry_t *)(ctx->dictionary->latest);
	if (0 != ctx->current)
	{
		return (forth_vocabulary_entry_t *)(((forth_wordlist_t *)(ctx->current))->latest);
	}
	
	// If all else fails just default to Forth.
	return (forth_vocabulary_entry_t *)(ctx->dictionary->forth_wl.latest);

}

void forth_SET_LATEST(forth_runtime_context_t *ctx, forth_vocabulary_entry_t *token)
{
	if (0 == ctx->dictionary)
	{
		forth_THROW(ctx, -21); // 	unsupported operation
	}

	if (0 == ctx->current)
	{
		forth_THROW(ctx, -51); // 	compilation word list changed
	}

	((forth_wordlist_t *)(ctx->current))->latest = (forth_cell_t)token;
	//ctx->dictionary->latest = (forth_cell_t)token;
	//ctx->dictionary->forth_wl.latest = (forth_cell_t)token;
}

// LATEST ( -- addr )
void forth_latest(forth_runtime_context_t *ctx)
{
	forth_PUSH(ctx, (forth_cell_t)forth_GET_LATEST(ctx));
}

// RECURSE ( -- )
void forth_recurse(forth_runtime_context_t *ctx)
{
	forth_cell_t xt = ctx->defining;

	if (0 == xt)
	{
		forth_THROW(ctx, -27); // invalid recursion
	}

	forth_COMPILE_COMMA(ctx, xt);
}

// CREATE ( "name" -- )
void forth_create(forth_runtime_context_t *ctx)
{
	forth_vocabulary_entry_t *entry;

	entry = forth_PARSE_NAME_AND_CREATE_ENTRY(ctx);
	entry->flags = FORTH_XT_FLAGS_ACTION_CREATE;
	forth_COMMA(ctx, 0); // Meaning.
	entry->link = (forth_cell_t)forth_GET_LATEST(ctx);
	forth_SET_LATEST(ctx, entry);
}

// >BODY ( xt -- addr )
void forth_to_body(forth_runtime_context_t *ctx)
{
	forth_CHECK_STACK_AT_LEAST(ctx, 1);

	if (FORTH_XT_FLAGS_ACTION_CREATE != (((forth_xt_t)ctx->sp[0])->flags & FORTH_XT_FLAGS_ACTION_MASK))
	{
		forth_THROW(ctx, -31); // >BODY used on non-CREATEd definition
	}

	ctx->sp[0] += sizeof(forth_vocabulary_entry_t);
}

// (does>) ( -- )
void forth_p_does(forth_runtime_context_t *ctx)
{
	forth_vocabulary_entry_t *entry = forth_GET_LATEST(ctx);
	entry->meaning = (forth_cell_t)&(ctx->ip[1]);
}

// DOES> C:( colon-sys1 -- colon-sys2 )
void forth_does(forth_runtime_context_t *ctx)
{
	if (0 == ctx->state)
	{
		forth_THROW(ctx, -14); // interpreting a compile-only word
	}

	forth_COMPILE_COMMA(ctx, forth_pDOES_xt); // (does>)
	forth_semicolon(ctx);
	forth_colon_noname(ctx);
	forth_nip(ctx);
}
#endif
// ---------------------------------------------------------------------------------------------------------------
void forth_PRINT_NAME(forth_runtime_context_t *ctx, forth_xt_t xt)
{
		if ((0 == xt->name) || (0 == strlen((const char *)xt->name)))
		{
			forth_TYPE0(ctx, "NONAME-XT-");
			forth_PUSH(ctx, (forth_cell_t)xt);
			forth_hdot(ctx);
		}
		else
		{
			forth_TYPE0(ctx, (const char *)(xt->name));
			forth_space(ctx);
		}
}

#if !defined(FORTH_WITHOUT_COMPILATION)
void forth_SEE_THREADED(forth_runtime_context_t *ctx, forth_xt_t xt)
{
	forth_cell_t *ip;
	forth_xt_t x;
	forth_cell_t len;
	forth_cell_t tmp;

	if ((0 == xt->name) || (0 == strlen((const char *)xt->name)))
	{
		forth_TYPE0(ctx, ":noname ");
	}
	else
	{
		forth_TYPE0(ctx, ": ");
		forth_PRINT_NAME(ctx, xt);
	}

	for (ip = &(xt->meaning); 0 != *ip; ip++)
	{
		x = *((forth_xt_t *)ip);

		if (forth_LIT_xt == x)
		{
			forth_PUSH(ctx, *++ip);
			forth_dot(ctx);
		}
		else if (forth_XLIT_xt == x)
		{
			forth_TYPE0(ctx, " ['] ");
			ip++;
			x = *((forth_xt_t *)ip);
			forth_PRINT_NAME(ctx, x);
		}
		else if (forth_SLIT_xt == x)
		{
			len = ip[1];
			forth_PUSH(ctx, (forth_cell_t)(ip + 2));
			forth_PUSH(ctx, len);
			forth_TYPE0(ctx, "s\" ");
			forth_type(ctx);
			forth_TYPE0(ctx, "\" ");
			tmp = ((forth_cell_t)(ip + 1)) + len; // Only + 1 (not 2) because the for() auto increments ip.
			tmp = FORTH_ALIGN(tmp);
			ip = (forth_cell_t *)tmp;
		}
		else if ((forth_BRANCH_xt == x) || (forth_0BRANCH_xt == x))
		{
			tmp = ip[1];
			ip += 1;
			forth_TYPE0(ctx, " [ ' ");
			forth_TYPE0(ctx, (const char *)x->name);
			forth_TYPE0(ctx, " COMPILE, ");
			forth_PUSH(ctx, tmp);
			forth_dot(ctx);
			forth_TYPE0(ctx, ", ] ");
			forth_space(ctx);
		}
		else if (forth_pDO_xt == x)
		{
			ip += 1;
			forth_TYPE0(ctx, "do ");
		}
		else if (forth_pqDO_xt == x)
		{
			ip += 1;
			forth_TYPE0(ctx, "?do ");
		}
		else if (forth_pLOOP_xt == x)
		{
			ip += 1;
			forth_TYPE0(ctx, "loop ");
		}
		else if (forth_ppLOOP_xt == x)
		{
			ip += 1;
			forth_TYPE0(ctx, "+loop ");
		}
		else if (forth_pDOES_xt == x)
		{
			ip+= 4;	// sizeof(forth_vocabulary_entry_struct) in cells.
			forth_TYPE0(ctx, "does> ");
		}
		else
		{
			forth_PRINT_NAME(ctx, x);
		}
	}
	forth_EMIT(ctx, ';');
}

#else

void forth_SEE_THREADED(forth_runtime_context_t *ctx, forth_xt_t xt)
{
	forth_cell_t *ip;
	forth_xt_t x;
	forth_cell_t len;
	forth_cell_t tmp;

	if ((0 == xt->name) || (0 == strlen((const char *)xt->name)))
	{
		forth_TYPE0(ctx, ":noname ");
	}
	else
	{
		forth_TYPE0(ctx, ": ");
		forth_PRINT_NAME(ctx, xt);
	}

	forth_TYPE0(ctx, " ... ;");
}
#endif

// SEE ( "name" -- )
void forth_SEE(forth_runtime_context_t *ctx, forth_xt_t xt)
{
	if (0 == xt)
	{
		return;
	}

	switch(xt->flags & FORTH_XT_FLAGS_ACTION_MASK)
	{
		case FORTH_XT_FLAGS_ACTION_PRIMITIVE:
			forth_TYPE0(ctx, (const char *)(xt->name));
			forth_TYPE0(ctx, " is a primitive.");
			break;

		case FORTH_XT_FLAGS_ACTION_CONSTANT:
			forth_PUSH(ctx, xt->meaning);
			forth_hdot(ctx);
			forth_TYPE0(ctx, "CONSTANT ");
			forth_TYPE0(ctx, (const char *)(xt->name));
			break;

		case FORTH_XT_FLAGS_ACTION_VARIABLE:
			forth_TYPE0(ctx, "VARIABLE ");
			forth_TYPE0(ctx, (const char *)(xt->name));
			break;
		
		case FORTH_XT_FLAGS_ACTION_CREATE:
			forth_TYPE0(ctx, "CREATE ");
			forth_TYPE0(ctx, (const char *)(xt->name));
			if (0 != xt->meaning)
			{
				forth_TYPE0(ctx, " ... DOES> ");
				forth_SEE(ctx, (forth_xt_t)(xt->meaning));
			}
			break;

		case FORTH_XT_FLAGS_ACTION_DEFER:
			forth_TYPE0(ctx, "DEFER ");
			forth_TYPE0(ctx, (const char *)(xt->name));
			break;

		case FORTH_XT_FLAGS_ACTION_THREADED:
			forth_SEE_THREADED(ctx, xt);
			break;

		default:
			forth_TYPE0(ctx, (const char *)(xt->name));
			forth_TYPE0(ctx, " ?????");
			break;
	}

	if (0 != (xt->flags & FORTH_XT_FLAGS_IMMEDIATE))
	{
		forth_TYPE0(ctx, " immediate");
	}
	forth_cr(ctx);
}

// SEE ( "name" -- )
void forth_see(forth_runtime_context_t *ctx)
{
	forth_tick(ctx);
	forth_SEE(ctx, (forth_xt_t)forth_POP(ctx));
}

// [DEFINED] ( "name" -- flag )
// True means the word is defined.
void forth_bracket_defined(forth_runtime_context_t *ctx)
{
    forth_parse_name(ctx);
    forth_find_name(ctx);
	ctx->sp[0] = ctx->sp[0] ? FORTH_TRUE : FORTH_FALSE;
    
}

// [UNDEFINED] ( "name" --  flag )
// True means the word is not defined.
void forth_bracket_undefined(forth_runtime_context_t *ctx)
{
    forth_parse_name(ctx);
    forth_find_name(ctx);
	ctx->sp[0] = ctx->sp[0] ? FORTH_FALSE : FORTH_TRUE;
}

// ' ( "name" -- xt )
void forth_tick(forth_runtime_context_t *ctx)
{
    forth_parse_name(ctx);
	ctx->symbol_addr = ctx->sp[1];
	ctx->symbol_length = ctx->sp[0];
    forth_find_name(ctx);

    if (0 == ctx->sp[0])
    {
        forth_THROW(ctx, -13);
    }
}

#if !defined(FORTH_WITHOUT_COMPILATION)
// ['] Compile: ( "name" -- ) Execute: ( -- xt )
void forth_bracket_tick(forth_runtime_context_t *ctx)
{
	forth_tick(ctx);
	forth_xliteral(ctx);
}
#endif

// CHAR ( "c" -- char )
void forth_char(forth_runtime_context_t *ctx)
{
	const char *p;

	forth_parse_name(ctx);
	if (0 == forth_POP(ctx))
	{
		forth_THROW(ctx, -18); // Parsed string overflow. (Not sure what is appropriate here.??)
	}

	p = (const char *)(ctx->sp[0]);
	ctx->sp[0] = (forth_cell_t)*p;
}

#if !defined(FORTH_WITHOUT_COMPILATION)
// [CHAR] Compile ( "char" -- ) Execution ( -- char )
void forth_bracket_char(forth_runtime_context_t *ctx)
{
	forth_char(ctx);
	forth_literal(ctx);
}
#endif

const forth_vocabulary_entry_t forth_wl_forth[] =
{
DEF_FORTH_WORD("(",  FORTH_XT_FLAGS_IMMEDIATE, forth_paren,         "( -- )"),
DEF_FORTH_WORD(".(", FORTH_XT_FLAGS_IMMEDIATE, forth_dot_paren,     "( -- )"),
DEF_FORTH_WORD("\\",  FORTH_XT_FLAGS_IMMEDIATE, forth_backslash,    "( -- )"),

DEF_FORTH_WORD("dup",        0, forth_dup,           "( x -- x x )"),
DEF_FORTH_WORD("?dup",       0, forth_question_dup,  "( 0 | x -- 0 | x x )"),
DEF_FORTH_WORD("nip",        0, forth_nip,           "( x y -- y )"),
DEF_FORTH_WORD("tuck",       0, forth_tuck,          "( x y -- y x y)"),
DEF_FORTH_WORD("rot",        0, forth_rot,           "( x y z -- y z x)"),
DEF_FORTH_WORD("-rot",       0, forth_mrot,          "( x y z -- z x y)"),
DEF_FORTH_WORD("pick",		 0, forth_pick,			 "( xu..x1 x0 u --  xu..x1 x0 xu)"),
DEF_FORTH_WORD("roll",       0, forth_roll,          "( xu xu-1 ... x0 u -- xu-1 ... x0 xu )"),
DEF_FORTH_WORD("swap",       0, forth_swap,          "( x y -- y x )"),
DEF_FORTH_WORD("@",          0, forth_fetch,         "( addr -- val )"),
DEF_FORTH_WORD("!",          0, forth_store,         "( val addr -- )"),
DEF_FORTH_WORD("+!",         0, forth_plus_store,    "( val addr -- )"),
DEF_FORTH_WORD("?",          0, forth_questionmark,  "( addr -- )"),
DEF_FORTH_WORD("c@",         0, forth_cfetch,        "( addr -- char )"),
DEF_FORTH_WORD("c!",         0, forth_cstore,        "( char addr -- )"),
DEF_FORTH_WORD("2@",         0, forth_2fetch,        "( addr -- x y )"),
DEF_FORTH_WORD("2!",         0, forth_2store,        "( x y addr -- )"),
DEF_FORTH_WORD("2dup",       0, forth_2dup,          "( x y -- x y x y )"),
DEF_FORTH_WORD("2drop",      0, forth_2drop,         "( x y -- )"),
DEF_FORTH_WORD("2swap",      0, forth_2swap,         "( x y a b -- a b x y )"),
DEF_FORTH_WORD("2over",      0, forth_2over,         "( x y a b -- x y a b x y )"),
DEF_FORTH_WORD("2rot",       0, forth_2rot,          "( x1 x2 x3 x4 x5 x6 -- x3 x4 x5 x6 x1 x2 )"),

#if !defined(FORTH_NO_DOUBLES)
DEF_FORTH_WORD("d0<",        0, forth_dzero_less,     "( d -- f )"),
DEF_FORTH_WORD("d0=",        0, forth_dzero_equals,   "( d -- f )"),
DEF_FORTH_WORD("d<",         0, forth_dless,          "( d1 d2 -- f )"),
DEF_FORTH_WORD("du<",        0, forth_duless,         "( du1 du2 -- f )"),
DEF_FORTH_WORD("d=",         0, forth_dequals,        "( d1 d2 -- f )"),
DEF_FORTH_WORD("d+",         0, forth_dplus,          "( d1 d2 -- d )"),
DEF_FORTH_WORD("d-",         0, forth_dminus,         "( d1 d2 -- d )"),
DEF_FORTH_WORD("m+",         0, forth_mplus,          "( d1 n -- d )"),
DEF_FORTH_WORD("d>s",        0, forth_drop,           "( d -- s )"),
DEF_FORTH_WORD("s>d",        0, forth_s_to_d,         "( s -- d )"),
DEF_FORTH_WORD("dnegate",    0, forth_dnegate,        "( d -- -d )"),
DEF_FORTH_WORD("dabs",    	 0, forth_dabs,        	  "( d -- |d| )"),
DEF_FORTH_WORD("dmin",    	 0, forth_dmin,        	  "( d1 d2 -- d )"),
DEF_FORTH_WORD("dmax",    	 0, forth_dmax,        	  "( d1 d2 -- d )"),
DEF_FORTH_WORD("D2*",     	 0, forth_d2mul,          "( d -- d*2 )"),
DEF_FORTH_WORD("D2/",     	 0, forth_d2div,          "( d -- d/2 )"),
DEF_FORTH_WORD("d.",    	 0, forth_ddot,        	  "( d -- )"),
#endif

DEF_FORTH_WORD(">r",         0, forth_to_r,          "( x -- )     R: ( -- x )"),
DEF_FORTH_WORD("r@",         0, forth_r_fetch,       "( -- x)      R: ( x -- x )"),
DEF_FORTH_WORD("r>",         0, forth_r_from,        "(  -- x )    R: ( x -- )"),
DEF_FORTH_WORD("2>r",        0, forth_2to_r,         "( x y -- )   R: ( -- x y)"),
DEF_FORTH_WORD("2r@",        0, forth_2r_fetch,      "( -- x y )   R: ( x y -- x y )"),
DEF_FORTH_WORD("2r>",        0, forth_2r_from,       "(  -- x y )  R: ( x y -- )"),
DEF_FORTH_WORD("n>r",        0, forth_n_to_r,        "( i*n n  -- ) R: ( -- i*n n )"),
DEF_FORTH_WORD("nr>",        0, forth_n_r_from,      "( -- i*n n )  R: ( i*n n -- )"),

DEF_FORTH_WORD("+",          0, forth_add,           "( x y -- x+y )"),
DEF_FORTH_WORD("-",          0, forth_subtract,      "( x y -- x-y )"),
DEF_FORTH_WORD("*",          0, forth_multiply,      "( x y -- x*y )"),
DEF_FORTH_WORD("/",          0, forth_divide,        "( x y -- x/y )"),
DEF_FORTH_WORD("mod",        0, forth_mod,           "( x y -- x%y )"),
DEF_FORTH_WORD("/mod",       0, forth_div_mod,      "( x y -- m q )"),
DEF_FORTH_WORD("*/",         0, forth_mult_div,      "( x y z -- q )"),
DEF_FORTH_WORD("*/mod",      0, forth_mult_div_mod,  "( x y z -- r q )"),
DEF_FORTH_WORD("um/mod",     0, forth_um_div_mod,    "( ud u -- m q )"),
DEF_FORTH_WORD("within",     0, forth_within,        "( x low high -- flag )"),
DEF_FORTH_WORD("min",        0, forth_min,           "( x y -- min )"),
DEF_FORTH_WORD("max",        0, forth_max,           "( x y -- max )"),
DEF_FORTH_WORD("and",        0, forth_and,           "( x y -- x&y )"),
DEF_FORTH_WORD("or",         0, forth_or,            "( x y -- x|y )"),
DEF_FORTH_WORD("xor",        0, forth_xor,           "( x y -- x^y )"),


DEF_FORTH_WORD("<>",         0, forth_not_equals,    "( x y -- flag )"),
DEF_FORTH_WORD("u<",         0, forth_uless,    	 "( x y -- flag )"),
DEF_FORTH_WORD("u>",         0, forth_ugreater,      "( x y -- flag )"),
DEF_FORTH_WORD("<",          0, forth_less,    	     "( x y -- flag )"),
DEF_FORTH_WORD(">",          0, forth_greater,       "( x y -- flag )"),

DEF_FORTH_WORD("0=",         0, forth_zero_equals,    "( x -- flag )"),
DEF_FORTH_WORD("0<>",        0, forth_zero_not_equals,"( x -- flag )"),
DEF_FORTH_WORD("0<",         0, forth_zero_less,      "( x -- flag )"),
DEF_FORTH_WORD("0>",         0, forth_zero_greater,  "( x -- flag )"),

DEF_FORTH_WORD("invert",     0, forth_invert,        "( x -- ~x )"),
DEF_FORTH_WORD("negate",     0, forth_negate,        "( x -- -x )"),
DEF_FORTH_WORD("abs",     	 0, forth_abs,        	 "( x -- |x| )"),
DEF_FORTH_WORD("lshift",     0, forth_lshift,        "( x sh -- x1 )"),
DEF_FORTH_WORD("rshift",     0, forth_rshift,        "( x sh -- x1 )"),
DEF_FORTH_WORD("m*",     	 0, forth_m_mult,        "( x y -- d )"),
DEF_FORTH_WORD("um*",     	 0, forth_um_mult,       "( x y -- d )"),
DEF_FORTH_WORD("2*",     	 0, forth_2mul,        	 "( x -- x*2 )"),
DEF_FORTH_WORD("2/",     	 0, forth_2div,        	 "( x -- x/2 )"),
DEF_FORTH_WORD("1+",     	 0, forth_1plus,         "( x -- x+1 )"),
DEF_FORTH_WORD("1-",     	 0, forth_1minus,        "( x -- x-1 )"),
DEF_FORTH_WORD("char+",      0, forth_1plus,         "( x -- x+1 )"),
DEF_FORTH_WORD("chars",      0, forth_noop,          "( x -- y )"),
DEF_FORTH_WORD("cell+",      0, forth_cell_plus,     "( x -- y )"),
DEF_FORTH_WORD("cells",      0, forth_cells,         "( x -- y )"),

DEF_FORTH_WORD("erase",      0, forth_erase,         "( c-addr len -- )"),
DEF_FORTH_WORD("blank",      0, forth_blank,         "( c-addr len -- )"),
DEF_FORTH_WORD("fill",       0, forth_fill,          "( c-addr len char -- )"),
DEF_FORTH_WORD("move",       0, forth_move,          "( src-addr dst-addr len -- )"),
DEF_FORTH_WORD("compare",	 0, forth_compare, 		 "( c-addr1 u1 c-addr2 u2 -- n )"),

DEF_FORTH_WORD("1", FORTH_XT_FLAGS_ACTION_CONSTANT, 1, "One"),
DEF_FORTH_WORD("0", FORTH_XT_FLAGS_ACTION_CONSTANT, 0, "Zero"),
DEF_FORTH_WORD("true", FORTH_XT_FLAGS_ACTION_CONSTANT, ~0, 0),
DEF_FORTH_WORD("false", FORTH_XT_FLAGS_ACTION_CONSTANT, 0, 0),

DEF_FORTH_WORD("space",      0, forth_space,         "( -- )" ),
DEF_FORTH_WORD("spaces",     0, forth_spaces,        "( n -- )"),
DEF_FORTH_WORD("emit",       0, forth_emit,          "( char -- )"),
DEF_FORTH_WORD("cr",         0, forth_cr,            "( -- )"),
DEF_FORTH_WORD("page",       0, forth_page,          "( -- )"),
DEF_FORTH_WORD("at-xy",      0, forth_at_xy,         "( x y -- )"),

DEF_FORTH_WORD(".",          0, forth_dot,           "( x -- )"),
DEF_FORTH_WORD("h.",         0, forth_hdot,          "( x -- )"),
DEF_FORTH_WORD("u.",         0, forth_udot,          "( x -- )"),
DEF_FORTH_WORD(".r",         0, forth_dotr,          "( x w -- )"),
DEF_FORTH_WORD("u.r",        0, forth_udotr,         "( u w -- )"),
DEF_FORTH_WORD(".s",         0, forth_dots,          "( -- )"),
DEF_FORTH_WORD("dump",       0, forth_dump,          "( addr count -- )"),

DEF_FORTH_WORD("<#",       	 0, forth_less_hash,     "( -- )"),
DEF_FORTH_WORD("hold",       0, forth_hold,          "( char -- )"),
DEF_FORTH_WORD("holds",      0, forth_holds,         "( c-addr len -- )"),
DEF_FORTH_WORD("sign",       0, forth_sign,          "( n -- )"),
DEF_FORTH_WORD("#",       	 0, forth_hash,          "( ud1 -- ud2 )"),
DEF_FORTH_WORD("#s",       	 0, forth_hash_s,        "( ud1 -- ud2 )"),
DEF_FORTH_WORD("#>",       	 0, forth_hash_greater,  "( ud -- c-addr len )"),

DEF_FORTH_WORD("key?",       0, forth_key_q,         "( -- flag )"),
DEF_FORTH_WORD("key",        0, forth_key,           "( -- key )"),
DEF_FORTH_WORD("ekey?",      0, forth_ekey_q,        "( -- flag )"),
DEF_FORTH_WORD("ekey",       0, forth_ekey,          "( -- key-event )" ),
DEF_FORTH_WORD("ekey>char",  0, forth_ekey2char,     "( key-event -- key-event false | char true )"),
DEF_FORTH_WORD("accept",     0, forth_accept,        "( c-addr len1 -- len2 )"),
DEF_FORTH_WORD("refill",     0, forth_refill,        "( -- flag )"),

DEF_FORTH_WORD(">in",      	 0, forth_to_in,      	 "( -- addr )"),
DEF_FORTH_WORD("save-input", 0, forth_save_input,    "( -- blk >in 2 )"),
DEF_FORTH_WORD("restore-input", 0, forth_restore_input, "( blk >in 2 -- flag )"),
DEF_FORTH_WORD("source",     0, forth_source,      	 "( -- c-addr length )"),
DEF_FORTH_WORD("source-id",  0, forth_source_id,     "( -- id )"),
DEF_FORTH_WORD("parse",      0, forth_parse,         "( char -- c-addr len )"),
DEF_FORTH_WORD(">number",    0, forth_to_number,     "( ud c-addr len -- ud1 c-addr1 len1 )"),
DEF_FORTH_WORD("\"",         0, forth_quot,          "( <string> -- c-addr len )"),
DEF_FORTH_WORD("s\"", FORTH_XT_FLAGS_IMMEDIATE, forth_squot,     "( <string> -- c-addr len )"),


DEF_FORTH_WORD("parse-name", 0, forth_parse_name,    "( \"name\" -- c-addr len )"),
DEF_FORTH_WORD("find-name",  0, forth_find_name,     "( c-addr len -- xt|0)"),
DEF_FORTH_WORD("'",          0, forth_tick,          "( \"name\" -- xt )"),

#if !defined(FORTH_WITHOUT_COMPILATION)
DEF_FORTH_WORD(".\"", FORTH_XT_FLAGS_IMMEDIATE, forth_dot_quote, "( <string> -- )"),
DEF_FORTH_WORD("postpone", FORTH_XT_FLAGS_IMMEDIATE, forth_postpone,  "( \"name\" -- )"),
DEF_FORTH_WORD("[']",FORTH_XT_FLAGS_IMMEDIATE, forth_bracket_tick,  "C: ( \"name\" -- ) R: ( -- xt )"),
DEF_FORTH_WORD("[char]", FORTH_XT_FLAGS_IMMEDIATE, forth_bracket_char, "C:( \"c\" -- ) R: ( -- char )"),
DEF_FORTH_WORD("[", FORTH_XT_FLAGS_IMMEDIATE, forth_left_bracket,   "Enter interpretation state."),
DEF_FORTH_WORD("]",      	               0, forth_right_bracket,  "Enter compilation state."),
DEF_FORTH_WORD("state",      0, forth_state,         "( -- addr )"),
DEF_FORTH_WORD("here",       0, forth_here,          "( -- addr )"),
DEF_FORTH_WORD("align",      0, forth_align,         "( --  )"),
DEF_FORTH_WORD("allot",      0, forth_allot,       	 "( n --  )"),
DEF_FORTH_WORD("c,",      	 0, forth_c_comma,       "( c --  )"),
DEF_FORTH_WORD(",",      	 0, forth_comma,         "( x --  )"),
#endif

DEF_FORTH_WORD("char",       0, forth_char,          "( \"c\" -- char )"),
DEF_FORTH_WORD(".error",     0, forth_print_error,   "( error_code -- )"),
DEF_FORTH_WORD("noop",       0, forth_noop,          "( -- )"),
DEF_FORTH_WORD("decimal",    0, forth_decimal,       "( -- )"),
DEF_FORTH_WORD("hex",    	 0, forth_hex,       	 "( -- )"),
DEF_FORTH_WORD("base",       0, forth_base,          "( -- addr )"),

#if !defined(FORTH_WITHOUT_COMPILATION)
DEF_FORTH_WORD("pad",        0, forth_here,          "( -- addr )"),
DEF_FORTH_WORD("unused",     0, forth_unused,        "( -- u )"),
#endif

DEF_FORTH_WORD("aligned",    0, forth_aligned,       "( addr -- a-addr )"),
DEF_FORTH_WORD("count",      0, forth_count,         "( c_addr -- c_addr+1 c )"),

#if !defined(FORTH_WITHOUT_COMPILATION)
DEF_FORTH_WORD("ahead", FORTH_XT_FLAGS_IMMEDIATE, forth_ahead,   "( -- )"),
DEF_FORTH_WORD("if",    FORTH_XT_FLAGS_IMMEDIATE, forth_if,   "( flag -- )"),
DEF_FORTH_WORD("else",  FORTH_XT_FLAGS_IMMEDIATE, forth_else, "( -- )"),
DEF_FORTH_WORD("then",  FORTH_XT_FLAGS_IMMEDIATE, forth_then, "( -- )"),
DEF_FORTH_WORD("begin", FORTH_XT_FLAGS_IMMEDIATE, forth_begin, "( -- )"),
DEF_FORTH_WORD("again", FORTH_XT_FLAGS_IMMEDIATE, forth_again, "( -- )"),
DEF_FORTH_WORD("until", FORTH_XT_FLAGS_IMMEDIATE, forth_until, "( f -- )"),
DEF_FORTH_WORD("while", FORTH_XT_FLAGS_IMMEDIATE, forth_while, "( f -- )"),
DEF_FORTH_WORD("repeat",FORTH_XT_FLAGS_IMMEDIATE, forth_repeat, "( -- )"),

DEF_FORTH_WORD("case",   FORTH_XT_FLAGS_IMMEDIATE, forth_case,    "( -- )"),
DEF_FORTH_WORD("of",     FORTH_XT_FLAGS_IMMEDIATE, forth_of,      "( x1 x2  -- x1 )"),
DEF_FORTH_WORD("endof",  FORTH_XT_FLAGS_IMMEDIATE, forth_endof,   "( -- )"),
DEF_FORTH_WORD("endcase",FORTH_XT_FLAGS_IMMEDIATE, forth_endcase, "( x -- )"),

DEF_FORTH_WORD("do",        FORTH_XT_FLAGS_IMMEDIATE, forth_do,     "( limit start -- )"),
DEF_FORTH_WORD("?do",       FORTH_XT_FLAGS_IMMEDIATE, forth_q_do,   "( limit start -- )"),
DEF_FORTH_WORD("i",     	0, forth_i,                         "( -- i )"),
DEF_FORTH_WORD("j",     	0, forth_j,                         "( -- j )"),
DEF_FORTH_WORD("unloop",	0, forth_unloop,                    "( -- )"),
DEF_FORTH_WORD("leave",		0, forth_leave,                     "( -- )"),
DEF_FORTH_WORD("loop",      FORTH_XT_FLAGS_IMMEDIATE, forth_loop,	"( -- )"),
DEF_FORTH_WORD("+loop",     FORTH_XT_FLAGS_IMMEDIATE, forth_plus_loop, "( inc -- )"),

DEF_FORTH_WORD("literal",  FORTH_XT_FLAGS_IMMEDIATE, forth_literal,       "( x --  )"),
DEF_FORTH_WORD("xliteral", FORTH_XT_FLAGS_IMMEDIATE, forth_xliteral,      "( xt --  )"),
DEF_FORTH_WORD("2literal", FORTH_XT_FLAGS_IMMEDIATE, forth_2literal,      "( x y --  )"),
DEF_FORTH_WORD("sliteral", FORTH_XT_FLAGS_IMMEDIATE, forth_sliteral,      "( c-addr count --  )"),
DEF_FORTH_WORD(":noname",    0, forth_colon_noname,  "( -- xt colon-sys )"),
DEF_FORTH_WORD(":",   		 0, forth_colon,         "( \"name\" -- colon-sys )"),
DEF_FORTH_WORD("recurse", FORTH_XT_FLAGS_IMMEDIATE, forth_recurse, "( -- )"),
DEF_FORTH_WORD(";", FORTH_XT_FLAGS_IMMEDIATE, forth_semicolon, "( colon-sys -- )"),
DEF_FORTH_WORD("immediate",  0, forth_immediate,     "( -- )"),
DEF_FORTH_WORD("latest",     0, forth_latest,        "( -- addr )"),
DEF_FORTH_WORD("variable",   0, forth_variable,      "( \"name\" -- )"),
DEF_FORTH_WORD("constant",   0, forth_constant,      "( val \"name\" -- )"),
DEF_FORTH_WORD("create",     0, forth_create,        "( \"name\" -- )"),
DEF_FORTH_WORD(">body",      0, forth_to_body,       "( xt -- addr )"),
DEF_FORTH_WORD("does>",  FORTH_XT_FLAGS_IMMEDIATE, forth_does, "( -- )"),
DEF_FORTH_WORD("cs-pick",	 0, forth_cspick,		 "Pick for the control-flow stack."),
DEF_FORTH_WORD("cs-roll",	 0, forth_csroll,		 "Roll for the control-flow stack."),
DEF_FORTH_WORD("exit",		 0, forth_exit,           "( -- )"),
#endif

DEF_FORTH_WORD("bl", FORTH_XT_FLAGS_ACTION_CONSTANT, FORTH_CHAR_SPACE, "( -- space )"),

DEF_FORTH_WORD("execute",    0, forth_execute,       "( xt -- )"),
DEF_FORTH_WORD("catch",      0, forth_catch,         "( xt -- code )"),
DEF_FORTH_WORD("throw",      0, forth_throw,         "( code -- )"),
DEF_FORTH_WORD("abort",      0, forth_abort,         "( -- )"),

#if !defined(FORTH_WITHOUT_COMPILATION)
DEF_FORTH_WORD("abort\"",     FORTH_XT_FLAGS_IMMEDIATE, forth_abort_quote,    "( flag -- )"),
#endif

DEF_FORTH_WORD("depth",      0, forth_depth,         "( -- depth )"),
DEF_FORTH_WORD("evaluate",   0, forth_evaluate,		 "( c-addr len -- )"),
DEF_FORTH_WORD("help",       0, forth_help,          "( -- )"),
DEF_FORTH_WORD("see",        0, forth_see,           "( \"name\"-- )"),
DEF_FORTH_WORD("quit",       0, forth_quit,          "( -- )"),
DEF_FORTH_WORD("[defined]",   FORTH_XT_FLAGS_IMMEDIATE, forth_bracket_defined, "( \"name\" -- flag )"),
DEF_FORTH_WORD("[undefined]", FORTH_XT_FLAGS_IMMEDIATE, forth_bracket_undefined, "( \"name\" -- flag )"),
DEF_FORTH_WORD("[if]",        FORTH_XT_FLAGS_IMMEDIATE, forth_bracket_if,   "( flag -- )"),
DEF_FORTH_WORD("[else]",      FORTH_XT_FLAGS_IMMEDIATE, forth_bracket_else, "( -- )"),
DEF_FORTH_WORD("[then]",      FORTH_XT_FLAGS_IMMEDIATE, forth_noop,         "( -- )"),
DEF_FORTH_WORD( "trace-on",  0, forth_trace_on,      "( -- )"),
DEF_FORTH_WORD( "trace-off", 0, forth_trace_off,     "( -- )"),
DEF_FORTH_WORD( "(trace)",   0, forth_paren_trace,   "( -- adr )"),
DEF_FORTH_WORD( "sp@",  	 0, forth_sp_fetch,      "( -- sp )"),
DEF_FORTH_WORD( "sp0",  	 0, forth_sp0,      	 "( -- sp0 )"),
DEF_FORTH_WORD( "sp!",  	 0, forth_sp_store,      "( sp -- )"),
DEF_FORTH_WORD( "rp@",  	 0, forth_rp_fetch,      "( -- rp )"),
DEF_FORTH_WORD( "rp!",  	 0, forth_rp_store,      "( rp -- )"),
DEF_FORTH_WORD( "rp0",  	 0, forth_rp0,      	 "( -- rp0 )"),
DEF_FORTH_WORD( ".version",  0, forth_print_version, "( -- ) Print the version number of the forth engine."),
DEF_FORTH_WORD( "forth-engine-version", FORTH_XT_FLAGS_ACTION_CONSTANT, FORTH_ENGINE_VERSION, "( -- v ) The version number of the forth engine."),
DEF_FORTH_WORD(0, 0, 0, 0)
};

// Some headers (used as execution tokens) that the system needs to refer to from C code.
// The order of these matters, so do not change this array unless you know what you are doing.
//
const forth_vocabulary_entry_t forth_wl_system[] =
{
DEF_FORTH_WORD("interpret",  0, forth_interpret,     "( -- )" ),							//  0
DEF_FORTH_WORD("drop",       0, forth_drop,          "( x -- )"),							//  1
DEF_FORTH_WORD("over",       0, forth_over,          "( x y -- x y x )"),					//  2
DEF_FORTH_WORD("=",          0, forth_equals,        "( x y -- flag )"),					//  3
DEF_FORTH_WORD("type",       0, forth_type,          "( addr count -- )"),					//  4
#if !defined(FORTH_WITHOUT_COMPILATION)
DEF_FORTH_WORD("compile,",   0, forth_comma,         "( xt --  )"),							//  5
DEF_FORTH_WORD("LIT",        0, forth_lit,           "( -- n )" ),							//  6
DEF_FORTH_WORD("XLIT",       0, forth_lit,           "( -- n )" ),							//  7
DEF_FORTH_WORD("SLIT",       0, forth_slit,          "( -- c-addr len )" ),					//  8
DEF_FORTH_WORD("BRANCH",	 0, forth_branch,		 " ( -- )"),							//  9
DEF_FORTH_WORD("0BRANCH",	 0, forth_0branch,		 " ( flag -- )"),						// 10
DEF_FORTH_WORD("(DO)",	     0, forth_do_rt,		 " ( limit start -- )"),				// 11
DEF_FORTH_WORD("(?DO)",	     0, forth_qdo_rt,		 " ( limit start -- )"),				// 12
DEF_FORTH_WORD("(LOOP)",	 0, forth_loop_rt,		 " ( -- )"),							// 13
DEF_FORTH_WORD("(+LOOP)",	 0, forth_plus_loop_rt,	 " ( inc -- )"),						// 14
DEF_FORTH_WORD("(does>)",    0, forth_p_does,        "( -- )"),								// 15
DEF_FORTH_WORD("(abort\")",  0, forth_pabortq,       "( f c-addr len -- )"),				// 16
DEF_FORTH_WORD( "(do-voc)",	 0, forth_do_voc,	 	 "( addr -- )"),						// 17
#endif
DEF_FORTH_WORD(0, 0, 0, 0)
};

// These refer to items in the array above, if you change one you are likely need to change the other.
//
const forth_xt_t forth_interpret_xt 		= (const forth_xt_t)&(forth_wl_system[0]);
const forth_xt_t forth_drop_xt 				= (const forth_xt_t)&(forth_wl_system[1]);
const forth_xt_t forth_over_xt 				= (const forth_xt_t)&(forth_wl_system[2]);
const forth_xt_t forth_equals_xt 			= (const forth_xt_t)&(forth_wl_system[3]);
const forth_xt_t forth_type_xt 				= (const forth_xt_t)&(forth_wl_system[4]);
#if !defined(FORTH_WITHOUT_COMPILATION)
const forth_xt_t forth_COMPILE_COMMA_xt		= (const forth_xt_t)&(forth_wl_system[5]);
const forth_xt_t forth_LIT_xt				= (const forth_xt_t)&(forth_wl_system[6]);
const forth_xt_t forth_XLIT_xt				= (const forth_xt_t)&(forth_wl_system[7]);
const forth_xt_t forth_SLIT_xt				= (const forth_xt_t)&(forth_wl_system[8]);
const forth_xt_t forth_BRANCH_xt			= (const forth_xt_t)&(forth_wl_system[9]);
const forth_xt_t forth_0BRANCH_xt			= (const forth_xt_t)&(forth_wl_system[10]);
const forth_xt_t forth_pDO_xt				= (const forth_xt_t)&(forth_wl_system[11]);
const forth_xt_t forth_pqDO_xt				= (const forth_xt_t)&(forth_wl_system[12]);
const forth_xt_t forth_pLOOP_xt				= (const forth_xt_t)&(forth_wl_system[13]);
const forth_xt_t forth_ppLOOP_xt			= (const forth_xt_t)&(forth_wl_system[14]);
const forth_xt_t forth_pDOES_xt				= (const forth_xt_t)&(forth_wl_system[15]);
const forth_xt_t forth_pABORTq_xt			= (const forth_xt_t)&(forth_wl_system[16]);
const forth_xt_t forth_DO_VOC_xt			= (const forth_xt_t)&(forth_wl_system[17]);
#endif
// -----------------------------------------------------------------------------------------------
// Get the size of the Forth runtime context structure.
// It is useful in code that does not include forth_internal.h but needs to e.g. allocate space for such a structure.
forth_cell_t Forth_GetContextSize(void)
{
	return (forth_cell_t)sizeof(forth_runtime_context_t);
}

// Initialize the Forth runtime context structure.
forth_scell_t Forth_InitContext(forth_runtime_context_t *ctx, const forth_context_init_data_t *init_data)
{
	int res;

	if ((0 == ctx) || (0 == init_data->data_stack) || (0 == init_data->return_stack) ||
	    (8 > init_data->data_stack_cell_count) || (8 > init_data->return_stack_cell_count))
	{
		return -1;
	}

	memset(ctx, 0, sizeof(forth_runtime_context_t));

	ctx->base = 10;			// Set base to decimal.
	ctx->ip = 0;
	ctx->sp_max = init_data->data_stack + (init_data->data_stack_cell_count - 1);
	ctx->sp_min = init_data->data_stack;
	ctx->sp0    = ctx->sp_max;
	ctx->sp     = ctx->sp_max;

	ctx->rp_max = init_data->return_stack + (init_data->return_stack_cell_count - 1);
	ctx->rp_min = init_data->return_stack;
	ctx->rp0    = ctx->rp_max;
	ctx->rp     = ctx->rp_max;

	forth_less_hash(ctx);	// Initialize the number formatting buffer so accidentally typed in HOLD, etc. does not crash.

#if !defined(FORTH_WITHOUT_COMPILATION)
	ctx->dictionary = init_data->dictionary;

	res = forth_InitSearchOrder(ctx, init_data->search_order, init_data->search_order_slots);

	if (0 > res)
	{
		return res;
	}
#endif
	return 0;
}

// Run the function which has the signature void f(forth_runtime_context_ctx *ctx) through Forth's CATCH.
// Return the code from CATCH (i.e. zero on success and a small negative integer on failure).
// It is useful to run C functions that call things int he Forth system that may throw an exeception.
// The optinonal NAME is the name to be printed on an execution trace (null is acceptable).
forth_scell_t Forth_Try(forth_runtime_context_t *ctx, forth_behavior_t f, char *name)
{
	forth_vocabulary_entry_t xt;

	if ((0 == ctx) || (0 == f))
	{
	    return -9; // Invalid memory address, is there anything better here?	
	}

	if ((0 == ctx->sp) || (0 == ctx->sp0) || (0 == ctx->sp_max) || (0 == ctx->sp_min) ||
	    (0 == ctx->rp) || (0 == ctx->rp0) || (0 == ctx->rp_max) || (0 == ctx->rp_min))
	{
		// Most likely CTX wasn't even initialized.
		return -9; // Invalid memory address, is there anything better here?
	}

//  Allow these to be uninitialized for now, perhaps it makes sense in some situations when the called code does not print anything.
//	if ((0 == ctx->write_string) || (0 == ctx->send_cr))
//	{
//		return -21; // Unsupported opration.
//	}

	xt.name		= (forth_cell_t)((0 != name) ? name : "Some-C-function");
	xt.flags 	= FORTH_XT_FLAGS_ACTION_PRIMITIVE;
	xt.meaning = (forth_cell_t)f;
	xt.link = 0;

	return forth_CATCH(ctx, &xt);
}

// Interpret the text in CMD.
// The command is passed as address and length (so we can interpret substrings inside some bigger buffer).
// A flag is passed to indicate if the data stack in the context needs to be emptied before running the command.
//
// This function returns 0 on success and a non-zero value (which is a code from CATCH/THROW) if an error has occurred.
//
forth_scell_t Forth(forth_runtime_context_t *ctx, const char *cmd, unsigned int cmd_length, int clear_stack)
{
    forth_cell_t res;
    jmp_buf frame;

    if (0 == cmd_length)
    {
        return 0;
    }

    if ((0 == ctx) || (0 == cmd))
    {
        return -9; // Invalid memory address, is there anything better here?
    }

	if ((0 == ctx->sp) || (0 == ctx->sp0) || (0 == ctx->sp_max) || (0 == ctx->sp_min) ||
	    (0 == ctx->rp) || (0 == ctx->rp0) || (0 == ctx->rp_max) || (0 == ctx->rp_min))
	{
		return -9; // Invalid memory address, is there anything better here?
	}

	if ((0 == ctx->write_string) || (0 == ctx->send_cr))
	{
		// These are mandatory and we cannot run without them since they are needed for all printing (including error messages).
		// If the application code does not want to print at all, and it wants to discard all output (which would be highly unusual)
		// at least some dummy version of these should be supplied.
		// These are NOT checked at run time (since they are assumed to be present), so we check for them here.
		return -21; // Unsupported operation.
	}

	if (0 == ctx->terminal_width)
	{
		ctx->terminal_width = 80; // Some reasonable default.
		ctx->terminal_col = 0;
	}

	if (0 == ctx->terminal_height)
	{
		ctx->terminal_height = 25; // Some reasonable default.
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

    ctx->to_in = 0;
    ctx->source_address = cmd;
    ctx->source_length = cmd_length;
	ctx->source_id = -1;
	ctx->blk = 0;

    res = forth_RUN_INTERPRET(ctx);

    return (int)res;
}
