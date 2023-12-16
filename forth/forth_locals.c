/*
* forth.c
*
*  Created on: Dec 12, 2023
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

#include <forth.h>
#include <forth_internal.h>
#include <string.h>

#if 1 || !defined(FORTH_WITHOUT_COMPILATION) && defined(FORTH_INCLUDE_LOCALS)

void forth_init_locals(forth_runtime_context_t *ctx)
{
    forth_cell_t count;

    if (0 == ctx->ip)
	{
		forth_THROW(ctx, -9); // Invalid address.
	}

    count = *(ctx->ip++);
    
    while (count--)
    {
        forth_RPUSH(ctx, forth_POP(ctx));
    }
}

//const forth_vocabulary_entry_t forth_init_locals_XT = DEF_FORTH_WORD("init-locals",        0, forth_init_locals,     "( n*x -- )");

const forth_vocabulary_entry_t forth_wl_local_support[] =
{
    DEF_FORTH_WORD("--init-locals--",   0, forth_init_locals,     "( n*x -- )"),            // 0
    DEF_FORTH_WORD("--uninitialized--", FORTH_XT_FLAGS_ACTION_CONSTANT, 0, 0),
    DEF_FORTH_WORD(0, 0, 0, 0)
};

const forth_xt_t forth_init_locals_xt   = (const forth_xt_t)&(forth_wl_local_support[0]);
const forth_xt_t forth_uninitialized_local_xt   = (const forth_xt_t)&(forth_wl_local_support[1]);

const forth_vocabulary_entry_t forth_wl_local_variables[] =
{
    DEF_FORTH_WORD("local[0x00]@", FORTH_XT_FLAGS_ACTION_LOCAL, 0x00, 0),
    DEF_FORTH_WORD("local[0x01]@", FORTH_XT_FLAGS_ACTION_LOCAL, 0x01, 0),
    DEF_FORTH_WORD("local[0x02]@", FORTH_XT_FLAGS_ACTION_LOCAL, 0x02, 0),
    DEF_FORTH_WORD("local[0x03]@", FORTH_XT_FLAGS_ACTION_LOCAL, 0x03, 0),
    DEF_FORTH_WORD("local[0x04]@", FORTH_XT_FLAGS_ACTION_LOCAL, 0x04, 0),
    DEF_FORTH_WORD("local[0x05]@", FORTH_XT_FLAGS_ACTION_LOCAL, 0x05, 0),
    DEF_FORTH_WORD("local[0x06]@", FORTH_XT_FLAGS_ACTION_LOCAL, 0x06, 0),
    DEF_FORTH_WORD("local[0x07]@", FORTH_XT_FLAGS_ACTION_LOCAL, 0x07, 0),
    DEF_FORTH_WORD("local[0x08]@", FORTH_XT_FLAGS_ACTION_LOCAL, 0x08, 0),
    DEF_FORTH_WORD("local[0x09]@", FORTH_XT_FLAGS_ACTION_LOCAL, 0x09, 0),
    DEF_FORTH_WORD("local[0x0a]@", FORTH_XT_FLAGS_ACTION_LOCAL, 0x0a, 0),
    DEF_FORTH_WORD("local[0x0b]@", FORTH_XT_FLAGS_ACTION_LOCAL, 0x0b, 0),
    DEF_FORTH_WORD("local[0x0c]@", FORTH_XT_FLAGS_ACTION_LOCAL, 0x0c, 0),
    DEF_FORTH_WORD("local[0x0d]@", FORTH_XT_FLAGS_ACTION_LOCAL, 0x0d, 0),
    DEF_FORTH_WORD("local[0x0e]@", FORTH_XT_FLAGS_ACTION_LOCAL, 0x0e, 0),
    DEF_FORTH_WORD("local[0x0f]@", FORTH_XT_FLAGS_ACTION_LOCAL, 0x0f, 0),

    DEF_FORTH_WORD("local[0x00]!", FORTH_XT_FLAGS_ACTION_LOCAL, FORTH_LOCALS_WRITE_MASK|0x00, 0),
    DEF_FORTH_WORD("local[0x01]!", FORTH_XT_FLAGS_ACTION_LOCAL, FORTH_LOCALS_WRITE_MASK|0x01, 0),
    DEF_FORTH_WORD("local[0x02]!", FORTH_XT_FLAGS_ACTION_LOCAL, FORTH_LOCALS_WRITE_MASK|0x02, 0),
    DEF_FORTH_WORD("local[0x03]!", FORTH_XT_FLAGS_ACTION_LOCAL, FORTH_LOCALS_WRITE_MASK|0x03, 0),
    DEF_FORTH_WORD("local[0x04]!", FORTH_XT_FLAGS_ACTION_LOCAL, FORTH_LOCALS_WRITE_MASK|0x04, 0),
    DEF_FORTH_WORD("local[0x05]!", FORTH_XT_FLAGS_ACTION_LOCAL, FORTH_LOCALS_WRITE_MASK|0x05, 0),
    DEF_FORTH_WORD("local[0x06]!", FORTH_XT_FLAGS_ACTION_LOCAL, FORTH_LOCALS_WRITE_MASK|0x06, 0),
    DEF_FORTH_WORD("local[0x07]!", FORTH_XT_FLAGS_ACTION_LOCAL, FORTH_LOCALS_WRITE_MASK|0x07, 0),
    DEF_FORTH_WORD("local[0x08]!", FORTH_XT_FLAGS_ACTION_LOCAL, FORTH_LOCALS_WRITE_MASK|0x08, 0),
    DEF_FORTH_WORD("local[0x09]!", FORTH_XT_FLAGS_ACTION_LOCAL, FORTH_LOCALS_WRITE_MASK|0x09, 0),
    DEF_FORTH_WORD("local[0x0a]!", FORTH_XT_FLAGS_ACTION_LOCAL, FORTH_LOCALS_WRITE_MASK|0x0a, 0),
    DEF_FORTH_WORD("local[0x0b]!", FORTH_XT_FLAGS_ACTION_LOCAL, FORTH_LOCALS_WRITE_MASK|0x0b, 0),
    DEF_FORTH_WORD("local[0x0c]!", FORTH_XT_FLAGS_ACTION_LOCAL, FORTH_LOCALS_WRITE_MASK|0x0c, 0),
    DEF_FORTH_WORD("local[0x0d]!", FORTH_XT_FLAGS_ACTION_LOCAL, FORTH_LOCALS_WRITE_MASK|0x0d, 0),
    DEF_FORTH_WORD("local[0x0e]!", FORTH_XT_FLAGS_ACTION_LOCAL, FORTH_LOCALS_WRITE_MASK|0x0e, 0),
    DEF_FORTH_WORD("local[0x0f]!", FORTH_XT_FLAGS_ACTION_LOCAL, FORTH_LOCALS_WRITE_MASK|0x0f, 0),

    DEF_FORTH_WORD(0, 0, 0, 0)
};

const forth_vocabulary_entry_t *forth_find_local(forth_runtime_context_t *ctx, const char *name, forth_cell_t len, int write)
{
    forth_dictionary_t *dict = ctx->dictionary;
    int i;

    if (0 == dict)
	{
		forth_THROW(ctx, -21); // Unsupported operation.
	}

    for (i = 0; i < dict->local_count; i++)
    {
        if (forth_COMPARE_NAMES(dict->local_names[i], (char *)name, len))
        {
            if (write)
            {
                return &forth_wl_local_variables[FORTH_LOCALS_MAX_COUNT + i];
            }
            else
            {
                return &forth_wl_local_variables[i];
            }
        }
    }
    return 0;
}

// (LOCAL) ( c-addr len -- )
void forth_paren_local(forth_runtime_context_t *ctx)
{
    forth_cell_t len = forth_POP(ctx);
    forth_cell_t name = forth_POP(ctx);
    forth_dictionary_t *dict = ctx->dictionary;
    int i;

    if (0 == dict)
	{
		forth_THROW(ctx, -21); // Unsupported operation.
	}

    if (len > (FORTH_LOCALS_NAME_MAX_LENGTH))
    {
        forth_THROW(ctx, -19); // Definition name too long.
    }

    if (0 == ctx->defining)
    {
        forth_THROW(ctx, -14); // Interpreting a compile-only word.
    }

    if (0 == len)
    {
        if (0 != dict->local_count)
        {
            ((forth_vocabulary_entry_t *)(ctx->defining))->flags |= FORTH_XT_FLAGS_LOCALS;
            forth_COMPILE_COMMA(ctx , forth_init_locals_xt);
            forth_COMMA(ctx, dict->local_count);
        }
    }
    else
    {
        for (i = 0; i < dict->local_count; i++)
        {
            if (forth_COMPARE_NAMES(dict->local_names[i], (char *)name, len))
            {
                //int forth_COMPARE_NAMES(const char *name, const char *input_word, int input_word_length)
                forth_THROW(ctx, -32); // Invalid name argument -- not sure what to throw if two locals have the same name.
            }
        }

        if (FORTH_LOCALS_MAX_COUNT <= dict->local_count)
        {
            forth_THROW(ctx, -21); // Unsupported opration. (Is there a better exception for this?)
        }

        memcpy(dict->local_names[dict->local_count], (void *)name, len);
        dict->local_names[dict->local_count][len] = 0;
        dict->local_count += 1;
    }
}

// LOCALS| ( "name...name |" -- )
void forth_locals_bar(forth_runtime_context_t *ctx)
{
    char *name;
    forth_cell_t len;

    if (0 == ctx->state)
    {
        forth_THROW(ctx, -14); // Interpreting a compile-only word.
    }

    while(1)
    {
        forth_parse_name(ctx);
        len = forth_POP(ctx);
        name = (char *)forth_POP(ctx);

        if (0 == len)
        {
            forth_THROW(ctx, -16); // Attempt to use zero-length string as a name.
        }

        if ((1 == len) && ('|' == name[0]))
        {
            forth_PUSH(ctx, 0);
            forth_PUSH(ctx, 0);
            forth_paren_local(ctx);
            return;
        }
        else
        {
            forth_PUSH(ctx, (forth_cell_t)name);
            forth_PUSH(ctx, len);
            forth_paren_local(ctx);
        }
    }
}

// {: ARGn ... ARG0 | LOCALn ... LOCAL0 -- outputs :}
void forth_brace_colon(forth_runtime_context_t *ctx)
{
    char *name;
    forth_cell_t len;
    forth_cell_t i;
    forth_cell_t arg_count = 0;
    forth_cell_t local_count = 0;
    int parsing_locals = 0;
    int parsing_output = 0;
    

    if (0 == ctx->state)
    {
        forth_THROW(ctx, -14); // Interpreting a compile-only word.
    }

    while(1)
    {
        forth_parse_name(ctx);
        len = forth_POP(ctx);
        name = (char *)forth_POP(ctx);

        if (0 == len)
        {
            forth_THROW(ctx, -16); // Attempt to use zero-length string as a name.
        }

        if ((1 == len) && ('|' == name[0]))
        {
            parsing_locals = 1;
            continue;
        }
        else if (2 == len)
        {
            if (('-' == name[0]) && ('-' == name[1]))
            {
                parsing_output = 1;
                continue;
            }
            else if ((':' == name[0]) && ('}' == name[1]))
            {
                break;
            }
        }

        if (!parsing_output)
        {
            forth_PUSH(ctx, (forth_cell_t)name);
            forth_PUSH(ctx, len);

            if (parsing_locals)
            {
                local_count++;
            }
            else
            {
                arg_count++;
            }
        }
    }

    if (0 != (arg_count + local_count))
    {
        for (i = 0; i < local_count; i++)
        {
            forth_COMPILE_COMMA(ctx, forth_uninitialized_local_xt);
            forth_paren_local(ctx);
        }

        for (i = 0; i < arg_count; i++)
        {
            forth_paren_local(ctx);
        }

        forth_PUSH(ctx, 0);
        forth_PUSH(ctx, 0);
        forth_paren_local(ctx);
    }
}
#endif
